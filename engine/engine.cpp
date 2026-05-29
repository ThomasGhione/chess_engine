#include "engine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string_view>
#include <utility>

#include <omp.h>

#include "../board/piece.hpp"
#include "../debug.hpp"

namespace engine {

namespace {

[[nodiscard]] bool iequalsAscii(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const char a = std::tolower(static_cast<unsigned char>(lhs[i]));
        const char b = std::tolower(static_cast<unsigned char>(rhs[i]));
        if (a != b) return false;
    }
    return true;
}

[[nodiscard]] bool resolveSearchApiMutexGuardDefault() noexcept {
    const char* envValue = std::getenv("CHESS_ENGINE_SEARCH_MUTEX_GUARD");
    if (envValue == nullptr || *envValue == '\0') {
        return true;
    }

    const std::string_view value(envValue);
    if (iequalsAscii(value, "0") || iequalsAscii(value, "off") || iequalsAscii(value, "false")) {
        return false;
    }
    if (iequalsAscii(value, "1") || iequalsAscii(value, "on") || iequalsAscii(value, "true")) {
        return true;
    }
    return true;
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

char Engine::promotionChoiceForMove(const chess::Board& board, const chess::Board::Move& move) noexcept {
    if (!chess::Coords::isInBounds(move.from) || !chess::Coords::isInBounds(move.to)) {
        return '\0';
    }

    const uint8_t fromPieceType = board.get(move.from) & chess::Board::MASK_PIECE_TYPE;
    if (fromPieceType != chess::Board::PAWN) {
        return '\0';
    }

    const bool isWhite = (board.getColor(move.from.index) == chess::Board::WHITE);
    const bool isPromotion = (move.to.rank() == chess::Board::promotionRank(isWhite));
    if (!isPromotion) {
        return '\0';
    }

    return (move.promotionPiece != '\0') ? move.promotionPiece : 'q';
}

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

Engine::Engine()
    : board(chess::Board())
    , searchRuntime{}
    , depth(searchRuntime.depth)
    , eval(searchRuntime.eval)
    , nodesSearched(searchRuntime.nodesSearched)
    , MAX_THREADS(searchRuntime.maxThreads) {
    ensureMagicTablesInitialized();
    searchApiMutexEnabled.store(resolveSearchApiMutexGuardDefault(), std::memory_order_relaxed);
    searchRuntime.maxThreads = omp_get_max_threads();
    bindSearchRuntime();
    this->tt.clear();
    this->openingBook.load("engine/komodo.bin");
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
    this->stopPondering();
    this->clearPonderResult();
    board = chess::Board();
    bestMove = chess::Board::Move{};
    moveHistory.clear();
    isPlayerWhite = true;
    gameResult = ONGOING;

    searchRuntime = Searcher::SearchRuntime{};
    searchRuntime.maxThreads = omp_get_max_threads();
    bindSearchRuntime();

    this->tt.clear();
    this->ponderCurrentDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedEvenDepth.store(0, std::memory_order_relaxed);
    this->ponderInterruptedDepth.store(0, std::memory_order_relaxed);
    this->ponderAspirationResearches.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailLow.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailHigh.store(0, std::memory_order_relaxed);
}

void Engine::setPonderDebugEnabled(bool enabled) noexcept {
    this->ponderDebugEnabled.store(enabled, std::memory_order_relaxed);
}

void Engine::setSearchApiMutexEnabled(bool enabled) noexcept {
    this->searchApiMutexEnabled.store(enabled, std::memory_order_release);
}

bool Engine::isSearchApiMutexEnabled() const noexcept {
    return this->searchApiMutexEnabled.load(std::memory_order_acquire);
}

bool Engine::isPonderDebugEnabled() const noexcept {
    return this->ponderDebugEnabled.load(std::memory_order_relaxed);
}

void Engine::clearPonderResult() noexcept {
    this->ponderRootHash = 0;
    this->ponderResultDepth = 0;
    this->ponderResultScore = 0;
    this->ponderResultMove = chess::Board::Move{};
    this->ponderResultReady = false;
}

uint64_t Engine::getPonderCurrentDepth() const noexcept {
    return this->ponderCurrentDepth.load(std::memory_order_relaxed);
}

uint64_t Engine::getPonderLastCompletedDepth() const noexcept {
    return this->ponderLastCompletedDepth.load(std::memory_order_relaxed);
}

uint64_t Engine::getPonderInterruptedDepth() const noexcept {
    return this->ponderInterruptedDepth.load(std::memory_order_relaxed);
}

__attribute__((hot))
bool Engine::movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece) noexcept {
    this->requestStopPondering();

    const bool result = (promotionPiece == '\0')
        ? this->board.move(from, to)
        : this->board.move(from, to, promotionPiece);

    if (result) [[likely]] {
        appendMoveHistoryEntry(from, to, promotionPiece);
    }

    this->updateGameResult();
    return result;
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
    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);
    this->nodesSearched = 0;
    this->tt.incrementGeneration();
    this->ponderCurrentDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedEvenDepth.store(0, std::memory_order_relaxed);
    this->ponderInterruptedDepth.store(0, std::memory_order_relaxed);
    this->ponderAspirationResearches.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailLow.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailHigh.store(0, std::memory_order_relaxed);

    if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
        DBG_LOG_STREAM("[PONDER] started from depth " << Engine::DEFAULTDEPTH << "\n");
    }

    const Searcher::IterativeSearchResult ponderResult = Searcher::runIterativeDeepening(
        rootBoard,
        this->searchRuntime,
        Engine::DEFAULTDEPTH,
        Engine::MAX_PLY);

    this->ponderCurrentDepth.store(ponderResult.completedDepth, std::memory_order_relaxed);
    this->ponderLastCompletedDepth.store(ponderResult.completedDepth, std::memory_order_relaxed);
    this->ponderLastCompletedEvenDepth.store(ponderResult.completedEvenDepth, std::memory_order_relaxed);
    this->ponderInterruptedDepth.store(ponderResult.interruptedDepth, std::memory_order_relaxed);
    this->ponderAspirationResearches.store(ponderResult.aspirationResearches, std::memory_order_relaxed);
    this->ponderAspirationFailLow.store(ponderResult.aspirationFailLow, std::memory_order_relaxed);
    this->ponderAspirationFailHigh.store(ponderResult.aspirationFailHigh, std::memory_order_relaxed);
    this->ponderResultMove = ponderResult.bestMove;
    this->ponderResultScore = ponderResult.bestScore;
    this->ponderResultDepth = ponderResult.completedDepth;
    this->ponderResultReady = ponderResult.hasLegalMoves
        && ponderResult.completedAnyDepth
        && chess::Coords::isInBounds(ponderResult.bestMove.from)
        && chess::Coords::isInBounds(ponderResult.bestMove.to);

    if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
        DBG_LOG_STREAM("[PONDER] ended. current depth: " << this->getPonderCurrentDepth()
                      << ", last completed depth: " << this->getPonderLastCompletedDepth()
                      << ", last even depth: " << this->ponderLastCompletedEvenDepth.load(std::memory_order_relaxed)
                      << ", interrupted depth: " << this->getPonderInterruptedDepth()
                      << ", asp retries: " << ponderResult.aspirationResearches
                      << ", fail-low: " << ponderResult.aspirationFailLow
                      << ", fail-high: " << ponderResult.aspirationFailHigh << "\n");
    }

    this->ponderingActive.store(false, std::memory_order_release);
}

void Engine::requestStopPondering() noexcept {
    if (!this->ponderingActive.load(std::memory_order_relaxed)
        && !this->ponderingThread.joinable()) {
        return;
    }

    this->ponderingStopRequested.store(true, std::memory_order_release);
    this->stopSearchRequested.store(true, std::memory_order_release);
}

bool Engine::tryUsePonderResult(uint64_t requestedDepth, chess::Board::Move& outMove) noexcept {
    const uint64_t targetDepth = (requestedDepth == 0)
        ? Engine::DEFAULTDEPTH
        : requestedDepth;

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
    if (!chess::Coords::isInBounds(ponderMove.from)) { // did the TT fail?
        ponderMove = getFallbackPonderMove(rootBoard, this->searchRuntime);
    }
    if (!chess::Coords::isInBounds(ponderMove.from)) return; // did the fallback fail too?

    if (!rootBoard.move(ponderMove.from, ponderMove.to, ponderMove.promotionPiece)) {
        rootBoard = this->board;
        ponderMove = getFallbackPonderMove(rootBoard, this->searchRuntime);
        if (!chess::Coords::isInBounds(ponderMove.from)
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
    bool hadActivePonder = false;
    {
        std::lock_guard<std::mutex> lock(this->ponderingMutex);
        hadActivePonder = this->ponderingActive.load(std::memory_order_relaxed) || this->ponderingWorkReady;
    }

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

    if (hadActivePonder && this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
        DBG_LOG_STREAM("[PONDER] stop requested. current depth: " << this->getPonderCurrentDepth()
                      << ", last completed depth: " << this->getPonderLastCompletedDepth()
                      << ", last even depth: " << this->ponderLastCompletedEvenDepth.load(std::memory_order_relaxed)
                      << ", interrupted depth: " << this->getPonderInterruptedDepth()
                      << ", asp retries: " << this->ponderAspirationResearches.load(std::memory_order_relaxed)
                      << ", fail-low: " << this->ponderAspirationFailLow.load(std::memory_order_relaxed)
                      << ", fail-high: " << this->ponderAspirationFailHigh.load(std::memory_order_relaxed) << "\n");
    }
}

void Engine::ponderWorkerLoop() noexcept {
    while (true) {
        chess::Board rootBoard;
        {
            std::unique_lock<std::mutex> lock(this->ponderingMutex);
            this->ponderingCv.wait(lock, [this] {
                return this->ponderingWorkReady || this->ponderingWorkerStopping;
            });
            if (this->ponderingWorkerStopping) break;
            rootBoard = std::move(this->ponderingBoard);
            this->ponderingWorkReady = false;
        }
        this->ponderLoop(std::move(rootBoard));
        this->ponderingCv.notify_all();
    }
    this->ponderingActive.store(false, std::memory_order_release);
    this->ponderingCv.notify_all();
}

void Engine::stopThinking() noexcept {
    this->requestStopPondering();
}

chess::Board::Move Engine::searchUCI(uint64_t requestedDepth) noexcept {
    std::unique_lock<std::mutex> searchApiGuard(this->searchApiMutex, std::defer_lock);
    if (this->searchApiMutexEnabled.load(std::memory_order_acquire)) {
        searchApiGuard.lock();
    }

    this->stopPondering();

    const uint64_t targetDepth = (requestedDepth == 0)
        ? Engine::DEFAULTDEPTH
        : requestedDepth;

    chess::Board::Move ponderMove{};
    if (this->tryUsePonderResult(targetDepth, ponderMove)) {
        this->bestMove = ponderMove;
        return this->bestMove;
    }

    if (this->openingEnabled.load(std::memory_order_relaxed)) {
        if (auto bookMove = this->openingBook.probe(this->board)) {
            this->bestMove = *bookMove;
            return this->bestMove;
        }
    }

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);
    this->clearPonderResult();

    chess::Board searchBoard = this->board;
    const chess::Board::Move candidate = Searcher::searchBestMove(searchBoard, this->searchRuntime, targetDepth);
    if (!chess::Coords::isInBounds(candidate.from) || !chess::Coords::isInBounds(candidate.to)) {
        this->bestMove = chess::Board::Move{};
        return this->bestMove;
    }

    this->bestMove = candidate;
    return this->bestMove;
}

chess::Board::Move Engine::searchUCI(const time::Limits& limits) noexcept {
    std::unique_lock<std::mutex> searchApiGuard(this->searchApiMutex, std::defer_lock);
    if (this->searchApiMutexEnabled.load(std::memory_order_acquire)) {
        searchApiGuard.lock();
    }

    this->stopPondering();

    const bool sideIsWhite =
        this->board.getActiveColor() == chess::Board::WHITE;
    const int movesPlayed = static_cast<int>(
        this->board.getFullMoveClock() > 0 ? this->board.getFullMoveClock() - 1 : 0);

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);

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

    chess::Board::Move ponderMove{};
    if (this->tryUsePonderResult(targetDepth, ponderMove)) {
        this->bestMove = ponderMove;
        return this->bestMove;
    }

    if (this->openingEnabled.load(std::memory_order_relaxed)) {
        if (auto bookMove = this->openingBook.probe(this->board)) {
            this->bestMove = *bookMove;
            return this->bestMove;
        }
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

    if (!chess::Coords::isInBounds(candidate.from) || !chess::Coords::isInBounds(candidate.to)) {
        this->bestMove = chess::Board::Move{};
        return this->bestMove;
    }

    this->bestMove = candidate;
    return this->bestMove;
}

void Engine::search(uint64_t requestedDepth) noexcept {
    std::unique_lock<std::mutex> searchApiGuard(this->searchApiMutex, std::defer_lock);
    if (this->searchApiMutexEnabled.load(std::memory_order_acquire)) {
        searchApiGuard.lock();
    }

    this->stopPondering();

    const uint64_t targetDepth = (requestedDepth == 0)
        ? Engine::DEFAULTDEPTH
        : requestedDepth;

    chess::Board::Move candidate{};
    if (!this->tryUsePonderResult(targetDepth, candidate)) {
        this->stopSearchRequested.store(false, std::memory_order_relaxed);
        this->searchInterrupted.store(false, std::memory_order_relaxed);
        this->clearPonderResult();

        if (this->openingEnabled.load(std::memory_order_relaxed)) {
            if (auto bookMove = this->openingBook.probe(this->board)) {
                candidate = *bookMove;
            }
        }
        if (!chess::Coords::isInBounds(candidate.from) || !chess::Coords::isInBounds(candidate.to)) {
            candidate = Searcher::searchBestMove(this->board, this->searchRuntime, targetDepth);
        }
    }

    if (!chess::Coords::isInBounds(candidate.from) || !chess::Coords::isInBounds(candidate.to)) {
        this->bestMove = chess::Board::Move{};
        this->updateGameResult();
        return;
    }

    const char promotionPiece = Engine::promotionChoiceForMove(this->board, candidate);
    const bool moveOk = this->board.move(candidate.from, candidate.to, promotionPiece);
    if (!moveOk) {
        this->bestMove = chess::Board::Move{};
        this->updateGameResult();
        return;
    }

    this->bestMove = candidate;
    this->updateGameResult();
    this->appendMoveHistoryEntry(candidate.from, candidate.to, candidate.promotionPiece);
    this->clearPonderResult();

    if (!this->isGameOver()) {
        this->startPondering();
    }

    DBG_ONLY(
        std::string moveStr = chess::Coords::toAlgebric(candidate.from) + chess::Coords::toAlgebric(candidate.to);
        if (candidate.promotionPiece != '\0') {
            moveStr += candidate.promotionPiece;
        }
        DBG_LOG_STREAM("Engine plays: " << moveStr << " (score: " << this->eval << ")\n");
    );
}

} // namespace engine
