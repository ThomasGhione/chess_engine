#include "../movegen/movegen.hpp"
#include "../engine.hpp"

#include "searcher.hpp"

namespace engine {

chess::Board::Move Engine::getBestMove(
    chess::Board& rootBoard,
    const MoveList<chess::Board::Move>& moves,
    bool usIsWhite,
    int32_t alpha,
    int32_t beta) noexcept {
    Searcher::SearchRuntime runtime{};
    runtime.nodesSearched = this->nodesSearched;
    runtime.depth = this->depth;
    runtime.eval = this->eval;
    runtime.maxThreads = this->MAX_THREADS;
    std::memcpy(runtime.killerMoves, this->killerMoves, sizeof(runtime.killerMoves));
    std::memcpy(runtime.history, this->history, sizeof(runtime.history));
    std::memcpy(runtime.counterMoves, this->counterMoves, sizeof(runtime.counterMoves));
    std::memcpy(runtime.captureHistory, this->captureHistory, sizeof(runtime.captureHistory));
    runtime.transpositionTable = &this->tt;
    runtime.stopSearchRequested = &this->stopSearchRequested;
    runtime.ponderingStopRequested = &this->ponderingStopRequested;
    runtime.searchInterrupted = &this->searchInterrupted;
    runtime.orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING;

    const chess::Board::Move bestMoveFromSearcher = Searcher::getBestMove(
        rootBoard,
        moves,
        usIsWhite,
        runtime,
        alpha,
        beta);

    this->nodesSearched = runtime.nodesSearched;
    this->depth = runtime.depth;
    this->eval = runtime.eval;
    std::memcpy(this->killerMoves, runtime.killerMoves, sizeof(this->killerMoves));
    std::memcpy(this->history, runtime.history, sizeof(this->history));
    std::memcpy(this->counterMoves, runtime.counterMoves, sizeof(this->counterMoves));
    std::memcpy(this->captureHistory, runtime.captureHistory, sizeof(this->captureHistory));

    return bestMoveFromSearcher;
}

Engine::IterativeSearchResult Engine::runIterativeDeepening(
    chess::Board& rootBoard,
    uint64_t startDepth,
    uint64_t targetDepth,
    bool allowStop) noexcept {
    Searcher::SearchRuntime runtime{};
    runtime.nodesSearched = this->nodesSearched;
    runtime.depth = this->depth;
    runtime.eval = this->eval;
    runtime.maxThreads = this->MAX_THREADS;
    std::memcpy(runtime.killerMoves, this->killerMoves, sizeof(runtime.killerMoves));
    std::memcpy(runtime.history, this->history, sizeof(runtime.history));
    std::memcpy(runtime.counterMoves, this->counterMoves, sizeof(runtime.counterMoves));
    std::memcpy(runtime.captureHistory, this->captureHistory, sizeof(runtime.captureHistory));
    runtime.transpositionTable = &this->tt;
    runtime.stopSearchRequested = &this->stopSearchRequested;
    runtime.ponderingStopRequested = &this->ponderingStopRequested;
    runtime.searchInterrupted = &this->searchInterrupted;
    runtime.orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING;

    const Searcher::IterativeSearchResult searcherResult = Searcher::runIterativeDeepening(
        rootBoard,
        runtime,
        startDepth,
        targetDepth,
        allowStop);

    this->nodesSearched = runtime.nodesSearched;
    this->depth = runtime.depth;
    this->eval = runtime.eval;
    std::memcpy(this->killerMoves, runtime.killerMoves, sizeof(this->killerMoves));
    std::memcpy(this->history, runtime.history, sizeof(this->history));
    std::memcpy(this->counterMoves, runtime.counterMoves, sizeof(this->counterMoves));
    std::memcpy(this->captureHistory, runtime.captureHistory, sizeof(this->captureHistory));

    IterativeSearchResult delegatedResult{};
    delegatedResult.hasLegalMoves = searcherResult.hasLegalMoves;
    delegatedResult.completedAnyDepth = searcherResult.completedAnyDepth;
    delegatedResult.startDepth = searcherResult.startDepth;
    delegatedResult.targetDepth = searcherResult.targetDepth;
    delegatedResult.completedIterations = searcherResult.completedIterations;
    delegatedResult.completedDepth = searcherResult.completedDepth;
    delegatedResult.completedEvenDepth = searcherResult.completedEvenDepth;
    delegatedResult.interruptedDepth = searcherResult.interruptedDepth;
    delegatedResult.aspirationResearches = searcherResult.aspirationResearches;
    delegatedResult.aspirationFailLow = searcherResult.aspirationFailLow;
    delegatedResult.aspirationFailHigh = searcherResult.aspirationFailHigh;
    delegatedResult.rootScoreBound = searcherResult.rootScoreBound;
    delegatedResult.bestMove = searcherResult.bestMove;
    delegatedResult.bestScore = searcherResult.bestScore;

    if (allowStop) {
        this->ponderCurrentDepth.store(delegatedResult.completedDepth, std::memory_order_relaxed);
        this->ponderLastCompletedDepth.store(delegatedResult.completedDepth, std::memory_order_relaxed);
        this->ponderLastCompletedEvenDepth.store(delegatedResult.completedEvenDepth, std::memory_order_relaxed);
        this->ponderInterruptedDepth.store(delegatedResult.interruptedDepth, std::memory_order_relaxed);
        this->ponderAspirationResearches.store(delegatedResult.aspirationResearches, std::memory_order_relaxed);
        this->ponderAspirationFailLow.store(delegatedResult.aspirationFailLow, std::memory_order_relaxed);
        this->ponderAspirationFailHigh.store(delegatedResult.aspirationFailHigh, std::memory_order_relaxed);
    }

    return delegatedResult;
}

void Engine::ponderLoop(chess::Board rootBoard) noexcept {
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
#ifdef DEBUG
        std::cout << "[PONDER] started from depth " << Engine::DEFAULTDEPTH << "\n";
#endif
    }

#ifdef DEBUG
    const IterativeSearchResult ponderResult = this->runIterativeDeepening(
        rootBoard,
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        static_cast<uint64_t>(Engine::MAX_PLY),
        true);

    if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
        std::cout << "[PONDER] ended. current depth: " << this->getPonderCurrentDepth()
                  << ", last completed depth: " << this->getPonderLastCompletedDepth()
                  << ", last even depth: " << this->ponderLastCompletedEvenDepth.load(std::memory_order_relaxed)
                  << ", interrupted depth: " << this->getPonderInterruptedDepth()
                  << ", asp retries: " << ponderResult.aspirationResearches
                  << ", fail-low: " << ponderResult.aspirationFailLow
                  << ", fail-high: " << ponderResult.aspirationFailHigh << "\n";
    }
#else
    this->runIterativeDeepening(
        rootBoard,
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        static_cast<uint64_t>(Engine::MAX_PLY),
        true);
#endif

    this->ponderingActive.store(false, std::memory_order_release);
}

void Engine::startPondering() noexcept {
    if (this->isGameOver()) return;

    this->stopPondering();

    const chess::Board rootBoard = this->board;
    this->ponderingStopRequested.store(false, std::memory_order_release);
    this->stopSearchRequested.store(false, std::memory_order_release);
    this->searchInterrupted.store(false, std::memory_order_release);
    this->ponderingActive.store(true, std::memory_order_release);

    try {
        this->ponderingThread = std::thread([this, rootBoard]() mutable {
            this->ponderLoop(rootBoard);
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
#ifdef DEBUG
        std::cout << "[PONDER] stop requested. current depth: " << this->getPonderCurrentDepth()
                  << ", last completed depth: " << this->getPonderLastCompletedDepth()
                  << ", last even depth: " << this->ponderLastCompletedEvenDepth.load(std::memory_order_relaxed)
                  << ", interrupted depth: " << this->getPonderInterruptedDepth()
                  << ", asp retries: " << this->ponderAspirationResearches.load(std::memory_order_relaxed)
                  << ", fail-low: " << this->ponderAspirationFailLow.load(std::memory_order_relaxed)
                  << ", fail-high: " << this->ponderAspirationFailHigh.load(std::memory_order_relaxed) << "\n";
#endif
    }
}

void Engine::stopThinking() noexcept {
    this->stopPondering();
}

// =========================================================================
// UCI-SAFE SEARCH: Find best move WITHOUT modifying engine board state.
// =========================================================================
// In UCI protocol, the GUI owns board state. The engine must NOT apply the
// best move on this->board, must NOT update gameResult, must NOT start
// pondering. The GUI will send a new "position" command before the next "go".
//
// This avoids:
// 1. Pondering polluting TT/history with entries from the wrong position
// 2. Board state desynchronization between engine and GUI
// 3. Non-deterministic move selection caused by stale pondering data
chess::Board::Move Engine::searchUCI(uint64_t requestedDepth) noexcept {
    this->stopPondering();

    const uint64_t targetDepth = (requestedDepth == 0)
        ? static_cast<uint64_t>(Engine::DEFAULTDEPTH)
        : requestedDepth;
    if (targetDepth == 0) return chess::Board::Move{};

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);

    this->tt.incrementGeneration();
    this->nodesSearched = 0;

    // History soft reset (same as search())
    int16_t* historyFlat = &this->history[0][0][0];
    constexpr int HISTORY_CELLS = 2 * 64 * 64;
    #pragma omp simd
    for (int i = 0; i < HISTORY_CELLS; ++i) {
        historyFlat[i] >>= 1;
    }

    // Search on a COPY of the board to avoid mutating this->board
    chess::Board searchBoard = this->board;
    IterativeSearchResult result = this->runIterativeDeepening(searchBoard, 1, targetDepth, false);
    this->depth = targetDepth;

    if (!result.hasLegalMoves || !result.completedAnyDepth) {
        MoveList<chess::Board::Move> fallbackMoves = MoveGenerator::generateLegalMoves(this->board);
        if (fallbackMoves.is_empty()) {
            return chess::Board::Move{};
        }
        this->bestMove = fallbackMoves[0];
        this->eval = this->evaluate(this->board);
        return this->bestMove;
    }

    this->bestMove = result.bestMove;
    this->eval = result.bestScore;

    // Do NOT apply the move on this->board.
    // Do NOT call updateGameResult().
    // Do NOT start pondering.
    return this->bestMove;
}

void Engine::search(uint64_t requestedDepth) noexcept {
    this->stopPondering();

    const uint64_t targetDepth = (requestedDepth == 0)
        ? static_cast<uint64_t>(Engine::DEFAULTDEPTH)
        : requestedDepth;
    if (targetDepth == 0) return;

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);

    // Increment TT generation to age old entries from previous searches
    this->tt.incrementGeneration();

    // Reset the nodes searched counter
    this->nodesSearched = 0;

    // ===================================================
    // HISTORY TABLE SOFT RESET - Age-based decay
    // ===================================================
    // Prevent stale data from dominating move ordering
    // Divide all history values by 2 at the start of each new search
    // This gives recent data more weight while preserving good moves
    int16_t* historyFlat = &this->history[0][0][0];
    constexpr int HISTORY_CELLS = 2 * 64 * 64;
    #pragma omp simd
    for (int i = 0; i < HISTORY_CELLS; ++i) {
        historyFlat[i] >>= 1; // Divide by 2
    }

    IterativeSearchResult result = this->runIterativeDeepening(this->board, 1, targetDepth, false);
    this->depth = targetDepth;

    if (!result.hasLegalMoves) {
        this->updateGameResult();
        return;
    }

    if (!result.completedAnyDepth) {
        MoveList<chess::Board::Move> fallbackMoves = MoveGenerator::generateLegalMoves(this->board);
        if (fallbackMoves.is_empty()) {
            this->updateGameResult();
            return;
        }
        result.bestMove = fallbackMoves[0];
        result.bestScore = this->evaluate(this->board);
        this->eval = result.bestScore;
    }

    const chess::Board::Move bestMove = result.bestMove;

    (void)this->board.move(
        bestMove.from,
        bestMove.to,
        isPromotionMove(this->board, bestMove)
            ? (bestMove.promotionPiece != '\0' ? bestMove.promotionPiece : 'q')
            : '\0');
    this->updateGameResult();
    this->bestMove = bestMove;
    this->eval = result.bestScore;

    this->appendMoveHistoryEntry(bestMove.from, bestMove.to, bestMove.promotionPiece);

    if (!this->isGameOver()) {
        this->startPondering();
    }

#ifdef DEBUG
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    if (bestMove.promotionPiece != '\0') {
        moveStr += bestMove.promotionPiece;
    }
    std::cout << "Engine plays: " << moveStr << " (score: " << this->eval << ")\n";
#endif
}

} // namespace engine
