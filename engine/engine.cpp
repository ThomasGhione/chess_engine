
#include "engine.hpp"

#include <iostream>

#include <omp.h>

#include "../debug.hpp"

namespace engine {

namespace {

[[nodiscard]] bool resolveSearchApiMutexGuardDefault() noexcept {
    const char* envValue = std::getenv("CHESS_ENGINE_SEARCH_MUTEX_GUARD");
    if (envValue == nullptr) return true;

    // Any value other than an explicit off-switch (incl. empty) enables the guard.
    const std::string_view value(envValue);
    return !(ascii::iequals(value, "0") || ascii::iequals(value, "off") || ascii::iequals(value, "false"));
}

[[nodiscard]] chess::Board::Move getTTPonderMove(const chess::Board& board, const TranspositionTable& tt) noexcept {
    uint16_t encodedMove = 0;
    if (!tt.probeMove(board.getHash(), encodedMove)) return {};

    const auto move = TranspositionTable::Entry::decodeMove(encodedMove);
    return {chess::Coords{move.from}, chess::Coords{move.to}, move.promo};
}

[[nodiscard]] chess::Board::Move getFallbackPonderMove(
    chess::Board& board,
    const Searcher::SearchRuntime& sourceRuntime) noexcept {

    Searcher::SearchRuntime runtime{};
    runtime.maxThreads = sourceRuntime.maxThreads;
    return Searcher::searchBestMove(board, runtime, 1);
}

} // namespace

void Engine::bindSearchRuntime() noexcept {
    searchRuntime.transpositionTable = &tt;
    searchRuntime.stopSearchRequested = &stopSearchRequested;
    searchRuntime.ponderingStopRequested = &ponderingStopRequested;
    searchRuntime.searchInterrupted = &searchInterrupted;
    searchRuntime.syzygyProber = &syzygyProber;
}

void Engine::ensureMagicTablesInitialized() noexcept {
    if (!magicTablesInitialized) {
        pieces::initMagicBitboards();
        magicTablesInitialized = true;
    }
}

Engine::Engine() {

    ensureMagicTablesInitialized();

    searchApiMutexEnabled.store(resolveSearchApiMutexGuardDefault(), std::memory_order_relaxed);
    searchRuntime.maxThreads = omp_get_max_threads();

    bindSearchRuntime();
    this->tt.clear();
    // The path is relative to the working directory; UCI clients can still
    // override at runtime via `setoption name BookFile value <path>`.
    if (!this->openingBook.load("engine/komodo.bin")) {
        std::cerr << "info string BookFile error: could not load 'engine/komodo.bin'"
                     " at startup (cwd-relative). Use 'setoption name BookFile value <path>' to override.\n";
    }
    this->ponderingThread = std::thread([this] { this->ponderWorkerLoop(); });
}

Engine::~Engine() noexcept {
    {
        std::lock_guard<std::mutex> lock(this->ponderingMutex);
        this->ponderingWorkerStopping = true;
        this->ponderingWorkReady = false;
    }
    this->requestStopPondering();
    this->ponderingCv.notify_one();
    if (this->ponderingThread.joinable()) this->ponderingThread.join();
}

void Engine::reset() noexcept {
    // Stop the watchdog first: if reset arrives mid-search, a still-running
    // watchdog would set stopSearchRequested *after* this reset returns and
    // abort whatever search starts next.
    this->timeManager.stop();
    this->stopPondering();
    this->clearPonderResult();
    board = chess::Board();
    bestMove = chess::Board::Move{};
    moveHistory.clear();
    isPlayerWhite = true;
    gameResult = GameResult::ONGOING;

    searchRuntime = Searcher::SearchRuntime{};
    searchRuntime.maxThreads = (requestedThreads > 0) ? requestedThreads : omp_get_max_threads();
    bindSearchRuntime();

    this->tt.clear();
}

void Engine::setSearchApiMutexEnabled(bool enabled) noexcept {
    this->searchApiMutexEnabled.store(enabled, std::memory_order_release);
}

bool Engine::isSearchApiMutexEnabled() const noexcept {
    return this->searchApiMutexEnabled.load(std::memory_order_acquire);
}

// Returns a guard already holding searchApiMutex when the guard is enabled,
// or an unlocked guard otherwise. Lets the search entry points share one line.
std::unique_lock<std::mutex> Engine::acquireSearchApiLock() noexcept {
    std::unique_lock<std::mutex> guard(this->searchApiMutex, std::defer_lock);
    if (this->searchApiMutexEnabled.load(std::memory_order_acquire)) {
        guard.lock();
    }
    return guard;
}

void Engine::clearPonderResult() noexcept {
    this->ponderRootHash = 0;
    this->ponderResultDepth = 0;
    this->ponderResultScore = 0;
    this->ponderResultMove = chess::Board::Move{};
    this->ponderResultReady = false;
}

void Engine::clearSearchStopFlags() noexcept {
    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);
}

// Book move when the opening book is enabled, else nullopt. Keeps the search
// entry points from repeating the enabled-then-probe dance.
std::optional<chess::Board::Move> Engine::probeOpeningBook() noexcept {
    if (!this->openingEnabled.load(std::memory_order_relaxed)) return std::nullopt;
    return this->openingBook.probe(this->board);
}

// A move available without a full search: a usable ponder result first, then
// the opening book. nullopt means the caller must actually search.
std::optional<chess::Board::Move> Engine::tryInstantMove(uint64_t targetDepth) noexcept {
    chess::Board::Move ponderMove{};
    if (this->tryUsePonderResult(targetDepth, ponderMove)) return ponderMove;
    return probeOpeningBook();
}

// Stores `candidate` as bestMove (normalising an out-of-bounds search result
// to the empty sentinel) and returns it.
chess::Board::Move Engine::commitSearchResult(const chess::Board::Move& candidate) noexcept {
    const bool playable = candidate.from.isValid()
                       && candidate.to.isValid();
    this->bestMove = playable ? candidate : chess::Board::Move{};
    return this->bestMove;
}

// Plays "move" on the engine's own board, recording it in the move history and
// refreshing the game result. Returns false (board untouched) when the move is
// out of bounds or illegal.
bool Engine::playMoveOnBoard(const chess::Board::Move& move) noexcept {
    if (!move.from.isValid() || !move.to.isValid()) return false;
    if (!this->board.move(move.from, move.to, move.promotionPiece)) return false;

    this->appendMoveHistoryEntry(move.from, move.to, move.promotionPiece);
    this->updateGameResult();
    return true;
}

__attribute__((hot))
bool Engine::movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece) noexcept {
    this->requestStopPondering();
    return this->playMoveOnBoard(chess::Board::Move{from, to, promotionPiece});
}

void Engine::appendMoveHistoryEntry(const chess::Coords& from, const chess::Coords& to, char promotionPiece) noexcept {
    const size_t appendLen = (promotionPiece == '\0') ? size_t{5} : size_t{6};

    if (moveHistory.size() + appendLen > MOVE_HISTORY_MAX_BYTES) {
        const size_t overflow = (moveHistory.size() + appendLen) - MOVE_HISTORY_MAX_BYTES;
        const size_t firstErase = std::min(overflow, moveHistory.size());
        moveHistory.erase(0, firstErase);

        const size_t lineEnd = moveHistory.find('\n');
        if (lineEnd != std::string::npos) {
            moveHistory.erase(0, lineEnd + 1);
        } else {
            moveHistory.clear();
        }
    }

    moveHistory.reserve(moveHistory.size() + appendLen);
    moveHistory += from.toString();
    moveHistory += to.toString();
    if (promotionPiece != '\0') {
        moveHistory += promotionPiece;
    }
    moveHistory += '\n';
}

void Engine::updateGameResult() noexcept {
    gameResult = GameResult::ONGOING;
    const uint8_t toMove = board.getActiveColor();
    if (board.kings_bb[0] == 0) {
        gameResult = GameResult::BLACK_WINS;
    } else if (board.kings_bb[1] == 0) {
        gameResult = GameResult::WHITE_WINS;
    } else if (board.isCheckmate(toMove)) {
        gameResult = (toMove == chess::Board::WHITE) ? GameResult::BLACK_WINS : GameResult::WHITE_WINS;
    } else if (board.isDraw(toMove)) {
        gameResult = GameResult::DRAW;
    }
}

void Engine::ponderLoop(chess::Board&& rootBoard) noexcept {
    this->ponderRootHash = rootBoard.getHash();
    this->clearSearchStopFlags();
    this->searchRuntime.nodesSearched = 0;
    this->tt.incrementGeneration();

    const Searcher::IterativeSearchResult ponderResult = Searcher::runIterativeDeepening(
        rootBoard, this->searchRuntime, Engine::DEFAULTDEPTH, Engine::MAX_PLY);

    this->ponderResultMove = ponderResult.bestMove;
    this->ponderResultScore = ponderResult.bestScore;
    this->ponderResultDepth = ponderResult.completedDepth;

    this->ponderResultReady = ponderResult.hasLegalMoves
        && ponderResult.completedAnyDepth
        && ponderResult.bestMove.from.isValid()
        && ponderResult.bestMove.to.isValid();

    this->ponderingActive.store(false, std::memory_order_release);
}

void Engine::requestStopPondering() noexcept {
    this->ponderingStopRequested.store(true, std::memory_order_release);
    this->stopSearchRequested.store(true, std::memory_order_release);
}

bool Engine::tryUsePonderResult(uint64_t targetDepth, chess::Board::Move& outMove) noexcept {
    if (!this->ponderResultReady) return false;
    if (this->ponderRootHash != this->board.getHash()) return false;
    if (this->ponderResultDepth < targetDepth) return false;

    outMove = this->ponderResultMove;
    this->searchRuntime.depth = this->ponderResultDepth;
    this->searchRuntime.eval = this->ponderResultScore;
    return true;
}

void Engine::startPondering() noexcept {
    if (this->isGameOver()) return;

    this->stopPondering();
    this->clearPonderResult();

    chess::Board rootBoard = this->board;
    auto ponderMove = getTTPonderMove(rootBoard, this->tt);
    if (!ponderMove.from.isValid()) { // did the TT fail?
        ponderMove = getFallbackPonderMove(rootBoard, this->searchRuntime);
    }
    if (!ponderMove.from.isValid()) return; // did the fallback fail too?

    if (!rootBoard.move(ponderMove.from, ponderMove.to, ponderMove.promotionPiece)) {
        rootBoard = this->board;
        ponderMove = getFallbackPonderMove(rootBoard, this->searchRuntime);
        if (!ponderMove.from.isValid()
            || !rootBoard.move(ponderMove.from, ponderMove.to, ponderMove.promotionPiece)) {
            return;
        }
    }

    this->ponderingStopRequested.store(false, std::memory_order_release);
    this->stopSearchRequested.store(false, std::memory_order_release);
    this->searchInterrupted.store(false, std::memory_order_release);
    this->ponderingActive.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(this->ponderingMutex);
        this->ponderingBoard = std::move(rootBoard);
        this->ponderingWorkReady = true;
    }
    this->ponderingCv.notify_one();
}

void Engine::stopPondering() noexcept {
    this->requestStopPondering();

    {
        std::unique_lock<std::mutex> lock(this->ponderingMutex);
        this->ponderingCv.wait(lock, [this] {
            return !this->ponderingWorkReady && !this->ponderingActive.load(std::memory_order_relaxed);
        });
    }

    this->ponderingActive.store(false, std::memory_order_release);
    this->ponderingStopRequested.store(false, std::memory_order_release);
    this->stopSearchRequested.store(false, std::memory_order_release);
    this->searchInterrupted.store(false, std::memory_order_release);
}

bool Engine::waitForPonderJob(chess::Board& outBoard) noexcept {
    std::unique_lock<std::mutex> lock(this->ponderingMutex);
    this->ponderingCv.wait(lock, [this] {
        return this->ponderingWorkReady || this->ponderingWorkerStopping;
    });
    if (this->ponderingWorkerStopping) return false;
    outBoard = std::move(this->ponderingBoard);
    this->ponderingWorkReady = false;
    return true;
}

void Engine::ponderWorkerLoop() noexcept {
    chess::Board rootBoard;
    while (waitForPonderJob(rootBoard)) {
        this->ponderLoop(std::move(rootBoard));
        this->ponderingCv.notify_all();
    }
    this->ponderingActive.store(false, std::memory_order_release);
    this->ponderingCv.notify_all();
}

void Engine::stopThinking() noexcept {
    this->requestStopPondering();
}

chess::Board::Move Engine::searchUCI(const time::Limits& limits) noexcept {
    auto searchApiGuard = acquireSearchApiLock();

    this->stopPondering();

    const bool sideIsWhite =
        this->board.getActiveColor() == chess::Board::WHITE;
    const int movesPlayed = static_cast<int>(
        this->board.getFullMoveClock() > 0 ? this->board.getFullMoveClock() - 1 : 0);

    this->clearSearchStopFlags();

    this->timeManager.init(limits, sideIsWhite, movesPlayed,
                           &this->stopSearchRequested);

    // Depth-bounded request keeps its cap; a time-managed search deepens
    // until the budget runs out (the watchdog trips the stop flag).
    uint64_t targetDepth;
    if (limits.maxDepth > 0) {
        targetDepth = static_cast<uint64_t>(limits.maxDepth);
    } else if (this->timeManager.useTimeManagement() || limits.infinite) {
        // Time-managed / infinite: deepen until the watchdog or an external
        // stop interrupts the search.
        targetDepth = static_cast<uint64_t>(Searcher::MAX_PLY);
    } else {
        // Ponder (or a bare `go`): this engine's `ponderhit` does not convert
        // a running search into a timed one, so a ponder search MUST be
        // bounded or it never returns a move on ponderhit and the engine
        // flags. Keep the pre-time-management fixed-depth behaviour.
        targetDepth = Engine::DEFAULTDEPTH;
    }

    if (auto instant = tryInstantMove(targetDepth)) {
        this->bestMove = *instant;
        return this->bestMove;
    }

    this->clearPonderResult();

    this->searchRuntime.timeManager = &this->timeManager;
    this->searchRuntime.maxNodes    = limits.maxNodes;
    this->timeManager.start();

    chess::Board searchBoard = this->board;
    const chess::Board::Move candidate =
        Searcher::searchBestMove(searchBoard, this->searchRuntime, targetDepth);

    this->timeManager.stop();
    this->searchRuntime.timeManager = nullptr;
    this->searchRuntime.maxNodes    = 0;

    return commitSearchResult(candidate);
}

void Engine::search(uint64_t requestedDepth) noexcept {
    // Terminal-mode play: compute the move via the one search entry point, then
    // apply it on our board and ponder the reply. searchUCI already set bestMove
    // (a maxDepth-only Limits runs to depth with no time management).
    const chess::Board::Move candidate =
        searchUCI(time::Limits{.maxDepth = static_cast<int64_t>(requestedDepth)});

    if (!this->playMoveOnBoard(candidate)) {
        this->bestMove = chess::Board::Move{};
        this->updateGameResult();
        return;
    }

    this->clearPonderResult();
    this->startPondering(); // no-op when the move just played ended the game

    DBG_ONLY(
        std::string moveStr = candidate.from.toString() + candidate.to.toString();
        if (candidate.promotionPiece != '\0') {
            moveStr += candidate.promotionPiece;
        }
        DBG_LOG_STREAM("Engine plays: " << moveStr << " (score: " << this->searchRuntime.eval << ")\n");
    );
}

} // namespace engine
