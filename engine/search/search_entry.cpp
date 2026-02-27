#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

static inline void rootNullWindow(bool usIsWhite, int64_t alpha, int64_t beta, int64_t& outAlpha, int64_t& outBeta) noexcept {
    outAlpha = usIsWhite ? alpha : (beta - 1);
    outBeta = usIsWhite ? (alpha + 1) : beta;
}

void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                          chess::Board::Move& bestMove, const chess::Board::Move& m) noexcept {
    // Update best score and move if this is better
    if (Engine::isBetter(score, bestScore, usIsWhite)) {
        bestScore = score;
        bestMove = m;
    }
    
    // Update alpha/beta bounds
    updateBound(score, alpha, beta, usIsWhite);
}

int64_t Engine::searchRootMoveScore(chess::Board& b, const chess::Board::Move& m, int64_t alpha, int64_t beta,
                                    int currPly, bool useTT, bool allowTTWrite, uint64_t* nodeCounter) noexcept {
    chess::Board::MoveState state;
    doMoveWithPromotion(b, m, state);
    const int64_t score = this->searchPosition(b, this->depth - 1, alpha, beta, currPly, useTT, allowTTWrite, nullptr, nodeCounter);
    b.undoMove(m, state);
    return score;
}

chess::Board::Move Engine::getBestMove(chess::Board& rootBoard, const MoveList<chess::Board::Move>& moves, bool usIsWhite) noexcept {
    return getBestMove(rootBoard, moves, usIsWhite, NEG_INF, POS_INF);
}

chess::Board::Move Engine::getBestMove(chess::Board& rootBoard, const MoveList<chess::Board::Move>& moves, bool usIsWhite, int64_t alpha, int64_t beta) noexcept {
    int64_t bestScore = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = moves[0];
    constexpr int currPly = 1;
    uint64_t localNodes = 0;
    bool searchedAnyMove = false;

    // Parallel YBWC is enabled only when:
    // - enough moves (>= 10) to amortize threading overhead
    // - sufficient depth (>= DEFAULTDEPTH - 2) for real speedup
    const bool useYBWC = (moves.size >= 10 && 
                          this->depth >= (Engine::DEFAULTDEPTH - 2));
    
    if (!useYBWC) {
        // Sequential search with PVS (Principal Variation Search)
        // First move: full window
        // Next moves: null window, then re-search if needed
        
        for (int i = 0; i < moves.size; ++i) {
            if (this->shouldAbortSearch()) {
                this->searchInterrupted.store(true, std::memory_order_relaxed);
                break;
            }

            const auto& m = moves[i];
            int64_t score = 0;
            if (i == 0) {
                // First move: search with full window (PV node)
                score = this->searchRootMoveScore(rootBoard, m, alpha, beta, currPly, true, true, &localNodes);
            } else {
                // Next moves: search with null window
                int64_t nullAlpha = 0, nullBeta = 0;
                rootNullWindow(usIsWhite, alpha, beta, nullAlpha, nullBeta);
                
                score = this->searchRootMoveScore(rootBoard, m, nullAlpha, nullBeta, currPly, true, true, &localNodes);
                
                // PVS re-search: if null-window fails, re-search with full window
                // White: re-search if score > alpha (null window failed high)
                // Black: re-search if score < beta (null window failed low)
                const bool shouldResearch = shouldResearchPVS(score, alpha, beta, usIsWhite);
                if (shouldResearch) {
                    score = this->searchRootMoveScore(rootBoard, m, alpha, beta, currPly, true, true, &localNodes);
                }
            }

            searchedAnyMove = true;
            if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                break;
            }

            // Update best move and alpha-beta bounds
            this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);

            // Beta cutoff check after updateMinMax
            if (isBetaCutoff(bestScore, alpha, beta, usIsWhite)) break;
        }
        this->nodesSearched += localNodes;
        if (searchedAnyMove) this->eval = bestScore;
        return bestMove;
    }

    // --- YBWC Parallel ---
    // First move: full-window search
    {
        const auto& firstMove = moves[0];
        const int64_t score = this->searchRootMoveScore(rootBoard, firstMove, alpha, beta, currPly, true, true, &localNodes);
        searchedAnyMove = true;
        this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, firstMove);
    }

    if (this->searchInterrupted.load(std::memory_order_relaxed)) {
        this->nodesSearched += localNodes;
        if (searchedAnyMove) this->eval = bestScore;
        return bestMove;
    }

    if (moves.size <= 1) [[unlikely]] {
        this->nodesSearched += localNodes;
        if (searchedAnyMove) this->eval = bestScore;
        return bestMove;
    }

    // All threads must see the same window to guarantee determinism
    const int64_t originalAlpha = alpha;
    const int64_t originalBeta = beta;

    std::array<int64_t, MAX_MOVES> threadScores;
    threadScores.fill(Engine::initialBest(usIsWhite));
    std::array<uint64_t, MAX_MOVES> threadNodeCounts {};

    // Task-based root parallelism (work-stealing, better load balance)
    // Bound the number of threads to MAX_THREADS and the number of moves
    int candidateThreads = std::max(1, static_cast<int>(moves.size) - 1);
    const int threadsToUse = std::min(this->MAX_THREADS, candidateThreads);

    if (threadsToUse <= 1) {
        for (int i = 1; i < moves.size; ++i) {
            if (this->shouldAbortSearch()) {
                this->searchInterrupted.store(true, std::memory_order_relaxed);
                break;
            }

            chess::Board threadBoard = rootBoard;
            const auto m = moves[i];
            uint64_t workerNodes = 0;
            const int64_t score = this->searchRootMoveScore(threadBoard, m, originalAlpha, originalBeta, currPly, false, false, &workerNodes);

            threadScores[i] = score;
            threadNodeCounts[i] = workerNodes;
            if (workerNodes > 0) searchedAnyMove = true;
            if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                break;
            }
        }
    } else {
        // Parallel region with chunked tasks to reduce task overhead and copies
        // We create tasks that process a contiguous range of moves. Each task copies the board ONCE
        // and evaluates all moves in the chunk sequentially. We use taskgroup to
        // wait for all tasks deterministically before merging results.
        const int totalJobs = static_cast<int>(moves.size - 1);
        int estimatedChunk = std::max(1, totalJobs / (threadsToUse * 4));
        const int chunk = std::min(16, estimatedChunk); // cap chunk size to avoid too-large tasks

        #pragma omp parallel num_threads(threadsToUse)        
        {
            #pragma omp single nowait
            {
                #pragma omp taskgroup
                {
                    for (int start = 1; start <= totalJobs; start += chunk) {
                        const int end = std::min(start + chunk, static_cast<int>(moves.size));
                        #pragma omp task firstprivate(start, end)
                        {
                            if (!this->shouldAbortSearch()) {
                                chess::Board threadBoard = rootBoard;

                                for (int i = start; i < end; ++i) {
                                    if (this->shouldAbortSearch()) {
                                        break;
                                    }
                                    const auto m = moves[i]; // local copy
                                    uint64_t workerNodes = 0;
                                    const int64_t score = this->searchRootMoveScore(threadBoard, m, originalAlpha, originalBeta, currPly, false, false, &workerNodes);

                                    threadScores[i] = score;
                                    threadNodeCounts[i] = workerNodes;
                                    if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                                        break;
                                    }
                                }
                            }
                        } // task
                    }
                } // taskgroup
            } // single
        } // parallel
    }

    // Merge results deterministically (sequential order, no race)
    for (int i = 1; i < moves.size; ++i) {
        if (this->searchInterrupted.load(std::memory_order_relaxed)) {
            break;
        }
        if (threadNodeCounts[i] == 0) continue;
        const int64_t score = threadScores[i];
        const auto& m = moves[i];
        if (Engine::isBetter(score, bestScore, usIsWhite)) {
            bestScore = score;
            bestMove = m;
        }
        searchedAnyMove = true;
        updateBound(score, alpha, beta, usIsWhite);
    }

    localNodes = std::accumulate(threadNodeCounts.begin() + 1, threadNodeCounts.end(), localNodes);
    this->nodesSearched += localNodes;
    if (searchedAnyMove) this->eval = bestScore;
    return bestMove;
}

void Engine::storeRootHashMove(const chess::Board& rootBoard, const chess::Board::Move& move, uint64_t depth, int64_t score) noexcept {
    if (!chess::Coords::isInBounds(move.from) || !chess::Coords::isInBounds(move.to)) {
        return;
    }

    const uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(
        move.from.index, move.to.index, move.promotionPiece);
    this->tt.store(rootBoard.getHash(), static_cast<uint8_t>(depth), score, tt::TranspositionTable::Entry::EXACT, encodedMove);
}

Engine::IterativeSearchResult Engine::runIterativeDeepening(chess::Board& rootBoard, uint64_t startDepth, uint64_t targetDepth, bool allowStop) noexcept {
    IterativeSearchResult result;
    MoveList<chess::Board::Move> moves = Engine::generateLegalMoves(rootBoard);
    if (moves.is_empty()) {
        const uint8_t toMove = rootBoard.getActiveColor();
        if (rootBoard.kings_bb[0] == 0) {
            result.bestScore = NEG_INF;
        } else if (rootBoard.kings_bb[1] == 0) {
            result.bestScore = POS_INF;
        } else if (rootBoard.isCheckmate(toMove)) {
            result.bestScore = (toMove == chess::Board::WHITE) ? NEG_INF : POS_INF;
        } else if (rootBoard.isDraw(toMove)) {
            result.bestScore = 0;
        } else {
            result.bestScore = this->evaluate(rootBoard);
        }
        this->eval = result.bestScore;
        return result;
    }

    result.hasLegalMoves = true;
    uint64_t interruptedDepth = 0;
    const bool searchBestMoveForWhite = (rootBoard.getActiveColor() == chess::Board::WHITE);
    chess::Board::Move bestMove = moves[0];
    int64_t prevScore = 0;
    const uint64_t firstDepth = std::max<uint64_t>(1, startDepth);
    const uint64_t maxDepth = std::max<uint64_t>(firstDepth, targetDepth);
    
    for (uint64_t currentDepth = firstDepth; currentDepth <= maxDepth; ++currentDepth) {
        if (allowStop && this->shouldAbortSearch()) {
            interruptedDepth = currentDepth;
            break;
        }

        this->depth = currentDepth;
        if (allowStop) {
            this->ponderCurrentDepth.store(currentDepth, std::memory_order_relaxed);
            if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
                std::cout << "[PONDER] current depth: " << currentDepth << "\n";
            }
        }
        
        // Move ordering: bring previous iteration's best move to the front
        if (result.completedAnyDepth) {
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i] == bestMove) {
                    chess::Board::Move::rotate(moves, i);
                    break;
                }
            }
        }

        this->searchInterrupted.store(false, std::memory_order_relaxed);
        bool iterationCompleted = true;
        chess::Board::Move candidateBestMove = moves[0];

        if (currentDepth <= 4 || !result.completedAnyDepth) {
            candidateBestMove = this->getBestMove(rootBoard, moves, searchBestMoveForWhite);
            if (allowStop && this->searchInterrupted.load(std::memory_order_relaxed)) {
                iterationCompleted = false;
            }
        } else {
            // Use aspiration window centered on previous iteration's score
            constexpr int64_t INITIAL_WINDOW = 50; // Start with ±50cp window
            int64_t windowDelta = INITIAL_WINDOW;
            
            int64_t aspAlpha = prevScore - windowDelta;
            int64_t aspBeta  = prevScore + windowDelta;
            
            // Search with narrow window
            candidateBestMove = this->getBestMove(rootBoard, moves, searchBestMoveForWhite, aspAlpha, aspBeta);
            if (allowStop && this->searchInterrupted.load(std::memory_order_relaxed)) {
                iterationCompleted = false;
            }
            
            // Check if search failed outside the window
            // If eval is outside [aspAlpha, aspBeta], we need to re-search with wider window
            while (iterationCompleted && (this->eval <= aspAlpha || this->eval >= aspBeta)) {
                // Widen the window exponentially
                windowDelta *= 2;
                
                // If window is too wide, fall back to full window
                if (windowDelta > 800) {
                    candidateBestMove = this->getBestMove(rootBoard, moves, searchBestMoveForWhite);
                    if (allowStop && this->searchInterrupted.load(std::memory_order_relaxed)) {
                        iterationCompleted = false;
                    }
                    break;
                }
                
                // Re-search with wider window
                if (this->eval <= aspAlpha) {
                    aspAlpha = prevScore - windowDelta;
                } else {
                    aspBeta = prevScore + windowDelta;
                }
                candidateBestMove = this->getBestMove(rootBoard, moves, searchBestMoveForWhite, aspAlpha, aspBeta);
                if (allowStop && this->searchInterrupted.load(std::memory_order_relaxed)) {
                    iterationCompleted = false;
                }
            }
        }

        if (!iterationCompleted) {
            if (allowStop) {
                interruptedDepth = currentDepth;
            }
            break;
        }

        bestMove = candidateBestMove;
        prevScore = this->eval;
        result.completedAnyDepth = true;
        result.completedDepth = currentDepth;
        result.bestMove = bestMove;
        result.bestScore = this->eval;
        this->storeRootHashMove(rootBoard, bestMove, currentDepth, this->eval);
        if (allowStop) {
            this->ponderLastCompletedDepth.store(currentDepth, std::memory_order_relaxed);
            if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
                std::cout << "[PONDER] last completed depth: " << currentDepth << "\n";
            }
        }
    }

    if (allowStop) {
        this->ponderInterruptedDepth.store(interruptedDepth, std::memory_order_relaxed);
    }

    return result;
}

void Engine::ponderLoop(chess::Board rootBoard) noexcept {
    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);
    this->nodesSearched = 0;
    this->tt.incrementGeneration();
    this->ponderCurrentDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedDepth.store(0, std::memory_order_relaxed);
    this->ponderInterruptedDepth.store(0, std::memory_order_relaxed);

    if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
        std::cout << "[PONDER] started from depth " << Engine::DEFAULTDEPTH << "\n";
    }

    // Keep extending depth while opponent is thinking: 10, 11, 12, ...
    (void)this->runIterativeDeepening(
        rootBoard,
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        static_cast<uint64_t>(Engine::MAX_PLY),
        true);

    if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
        std::cout << "[PONDER] ended. current depth: " << this->getPonderCurrentDepth()
                  << ", last completed depth: " << this->getPonderLastCompletedDepth()
                  << ", interrupted depth: " << this->getPonderInterruptedDepth() << "\n";
    }

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
        std::cout << "[PONDER] stop requested. current depth: " << this->getPonderCurrentDepth()
                  << ", last completed depth: " << this->getPonderLastCompletedDepth()
                  << ", interrupted depth: " << this->getPonderInterruptedDepth() << "\n";
    }
}

void Engine::stopThinking() noexcept {
    this->stopPondering();
}

void Engine::search(uint64_t requestedDepth) noexcept {
    this->stopPondering();

    const uint64_t targetDepth = std::max<uint64_t>(
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        requestedDepth);
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
    int32_t* historyFlat = &this->history[0][0][0];
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
        MoveList<chess::Board::Move> fallbackMoves = Engine::generateLegalMoves(this->board);
        if (fallbackMoves.is_empty()) {
            this->updateGameResult();
            return;
        }
        result.bestMove = fallbackMoves[0];
        result.bestScore = this->evaluate(this->board);
        this->eval = result.bestScore;
    }

    const chess::Board::Move bestMove = result.bestMove;

    (void)this->board.move(bestMove.from, bestMove.to, 
        isPromotionMove(this->board, bestMove) ? (bestMove.promotionPiece != '\0' ? bestMove.promotionPiece : 'q') : '\0');
    this->updateGameResult();
    this->bestMove = bestMove;
    this->eval = result.bestScore;

    this->moveHistory += bestMove.from.toString() + bestMove.to.toString();
    this->moveHistory += bestMove.promotionPiece == '\0' ? "\n" : std::string(1, bestMove.promotionPiece) + "\n";

    if (!this->isGameOver()) {
        this->startPondering();
    }

#ifdef DEBUG
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    if (bestMove.promotionPiece != '\0') {
        moveStr += bestMove.promotionPiece;
    }
    std::cout << "Engine plays: " << moveStr << " (score: " << this->eval << ")\n";
    // std::cout << "[DEBUG] TT probes: " << ttProbes << ", hits: " << ttHits << "\n";

#endif
}

} // namespace engine
