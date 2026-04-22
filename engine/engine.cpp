#include "engine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string_view>
#include <utility>

#include <omp.h>

#include "../board/piece.hpp"
#include "../debug_timer.hpp"
#include "eval/evaluator.hpp"

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
    searchRuntime.orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING;
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
}

Engine::Engine(const std::string& fen)
    : Engine() {
    board = chess::Board(fen);
}

Engine::~Engine() noexcept {
    this->stopPondering();
}

void Engine::reset() noexcept {
    this->stopPondering();
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
    this->stopPondering();

    const bool result = (promotionPiece == '\0')
        ? this->board.move(from, to)
        : this->board.move(from, to, promotionPiece);

    if (result) [[likely]] {
        appendMoveHistoryEntry(from, to, promotionPiece);
    }

    this->updateGameResult();
    return result;
}

int32_t Engine::evaluate(const chess::Board& board) noexcept {
    return Evaluator::evaluate(board);
}

int32_t Engine::evaluateTrace(const chess::Board& board) noexcept {
    return Evaluator::evaluateTrace(board);
}

int32_t Engine::evaluateCheckmate(const chess::Board& board) noexcept {
    return Evaluator::evaluateCheckmate(board);
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
        Engine::MAX_PLY,
        true);

    this->ponderCurrentDepth.store(ponderResult.completedDepth, std::memory_order_relaxed);
    this->ponderLastCompletedDepth.store(ponderResult.completedDepth, std::memory_order_relaxed);
    this->ponderLastCompletedEvenDepth.store(ponderResult.completedEvenDepth, std::memory_order_relaxed);
    this->ponderInterruptedDepth.store(ponderResult.interruptedDepth, std::memory_order_relaxed);
    this->ponderAspirationResearches.store(ponderResult.aspirationResearches, std::memory_order_relaxed);
    this->ponderAspirationFailLow.store(ponderResult.aspirationFailLow, std::memory_order_relaxed);
    this->ponderAspirationFailHigh.store(ponderResult.aspirationFailHigh, std::memory_order_relaxed);

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

void Engine::startPondering() noexcept {
    if (this->isGameOver()) return;

    this->stopPondering();

    this->ponderingStopRequested.store(false, std::memory_order_release);
    this->stopSearchRequested.store(false, std::memory_order_release);
    this->searchInterrupted.store(false, std::memory_order_release);
    this->ponderingActive.store(true, std::memory_order_release);

    try {
        this->ponderingThread = std::thread([this, rootBoard = chess::Board(this->board)]() mutable {
            this->ponderLoop(std::move(rootBoard));
        });
    } catch (...) {
        this->ponderingActive.store(false, std::memory_order_release);
        this->ponderingStopRequested.store(false, std::memory_order_release);
        this->stopSearchRequested.store(false, std::memory_order_release);
    }
}

void Engine::stopPondering() noexcept {
    const bool hadActivePonder = this->ponderingActive.load(std::memory_order_relaxed)
        || this->ponderingThread.joinable();

    this->ponderingStopRequested.store(true, std::memory_order_release);
    this->stopSearchRequested.store(true, std::memory_order_release);

    if (this->ponderingThread.joinable()) {
        this->ponderingThread.join();
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

void Engine::stopThinking() noexcept {
    this->stopPondering();
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
    if (targetDepth == 0) return chess::Board::Move{};

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);

    chess::Board searchBoard = this->board;
    const chess::Board::Move candidate = Searcher::searchBestMove(searchBoard, this->searchRuntime, targetDepth);
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
    if (targetDepth == 0) return;

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);

    const chess::Board::Move candidate = Searcher::searchBestMove(this->board, this->searchRuntime, targetDepth);
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
