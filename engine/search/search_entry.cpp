#include "../engine.hpp"
#include "../tt.hpp"

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

chess::Board::Move Engine::getBestMove(const MoveList<chess::Board::Move>& moves, bool usIsWhite) noexcept {
    return getBestMove(moves, usIsWhite, NEG_INF, POS_INF);
}

chess::Board::Move Engine::getBestMove(const MoveList<chess::Board::Move>& moves, bool usIsWhite, int64_t alpha, int64_t beta) noexcept {
    int64_t bestScore = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = moves[0];
    constexpr int currPly = 1;
    uint64_t localNodes = 0;

    // Parallel YBWC is enabled only when:
    // - enough moves (>= 10) to amortize threading overhead
    // - sufficient depth (>= DEFAULTDEPTH - 2) for real speedup
    const bool useYBWC = (moves.size >= 10 && 
                          this->depth >= (Engine::DEFAULTDEPTH - 2));
    
    if (!useYBWC) {
        // Sequential search con PVS (Principal Variation Search)
        // First move: full window
        // Next moves: null window, then re-search if needed
        
        for (int i = 0; i < moves.size; ++i) {
            const auto& m = moves[i];
            int64_t score = 0;
            if (i == 0) {
                // First move: search with full window (PV node)
                score = this->searchRootMoveScore(this->board, m, alpha, beta, currPly, true, true, &localNodes);
            } else {
                // Next moves: search with null window
                int64_t nullAlpha = 0, nullBeta = 0;
                rootNullWindow(usIsWhite, alpha, beta, nullAlpha, nullBeta);
                
                score = this->searchRootMoveScore(this->board, m, nullAlpha, nullBeta, currPly, true, true, &localNodes);
                
                // PVS re-search: if null-window fails, re-search with full window
                // White: re-search if score > alpha (null window failed high)
                // Black: re-search if score < beta (null window failed low)
                const bool shouldResearch = shouldResearchPVS(score, alpha, beta, usIsWhite);
                if (shouldResearch) {
                    score = this->searchRootMoveScore(this->board, m, alpha, beta, currPly, true, true, &localNodes);
                }
            }

            // Update best move and alpha-beta bounds
            this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);

            // Beta cutoff check after updateMinMax
            if (isBetaCutoff(bestScore, alpha, beta, usIsWhite)) break;
        }
        this->nodesSearched += localNodes;
        this->eval = bestScore;
        return bestMove;
    }

    // --- YBWC Parallel ---
    // First move: full-window search
    {
        const auto& firstMove = moves[0];
        const int64_t score = this->searchRootMoveScore(this->board, firstMove, alpha, beta, currPly, true, true, &localNodes);
        this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, firstMove);
    }

    if (moves.size <= 1) [[unlikely]] {
        this->nodesSearched += localNodes;
        this->eval = bestScore;
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
        // Sequential fallback (avoid OpenMP overhead)
        for (int i = 1; i < moves.size; ++i) {
            chess::Board threadBoard = this->board;
            const auto m = moves[i]; // copy to avoid referencing container inside tasks
            uint64_t workerNodes = 0;
            const int64_t score = this->searchRootMoveScore(threadBoard, m, originalAlpha, originalBeta, currPly, false, false, &workerNodes);

            threadScores[i] = score;
            threadNodeCounts[i] = workerNodes;
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
                            chess::Board threadBoard = this->board;

                            for (int i = start; i < end; ++i) {
                                const auto m = moves[i]; // local copy
                                uint64_t workerNodes = 0;
                                const int64_t score = this->searchRootMoveScore(threadBoard, m, originalAlpha, originalBeta, currPly, false, false, &workerNodes);

                                threadScores[i] = score;
                                threadNodeCounts[i] = workerNodes;
                            }
                        } // task
                    }
                } // taskgroup
            } // single
        } // parallel
    }

    // Merge results deterministically (sequential order, no race)
    for (int i = 1; i < moves.size; ++i) {
        const int64_t score = threadScores[i];
        const auto& m = moves[i];
        if (Engine::isBetter(score, bestScore, usIsWhite)) {
            bestScore = score;
            bestMove = m;
        }
        updateBound(score, alpha, beta, usIsWhite);
    }

    localNodes = std::accumulate(threadNodeCounts.begin() + 1, threadNodeCounts.end(), localNodes);
    this->nodesSearched += localNodes;
    this->eval = bestScore;
    return bestMove;
}

void Engine::search(uint64_t depth) noexcept {
    if (depth == 0) return;

    // Increment TT generation to age old entries from previous searches
    // This ensures the replacement policy favors fresh entries
    this->tt.incrementGeneration();

    MoveList<chess::Board::Move> moves = Engine::generateLegalMoves(this->board);
    if (moves.is_empty()) {
        // Root terminal position: update game state explicitly.
        // evaluate() does not handle stalemate, so set eval from game result.
        this->updateGameResult();
        switch (this->gameResult) {
            case GameResult::DRAW:
                this->eval = 0;
                break;
            case GameResult::WHITE_WINS:
                this->eval = POS_INF;
                break;
            case GameResult::BLACK_WINS:
                this->eval = NEG_INF;
                break;
            case GameResult::ONGOING:
            default:
                this->eval = this->evaluate(this->board);
                break;
        }
        return;
    }

    // Reset the nodes searched counter
    this->nodesSearched = 0; 

    // ===================================================
    // HISTORY TABLE SOFT RESET - Age-based decay
    // ===================================================
    // Prevent stale data from dominating move ordering
    // Divide all history values by 2 at the start of each new search
    // This gives recent data more weight while preserving good moves
    int* historyFlat = &this->history[0][0][0];
    constexpr int HISTORY_CELLS = 2 * 64 * 64;
    #pragma omp simd
    for (int i = 0; i < HISTORY_CELLS; ++i) {
        historyFlat[i] >>= 1; // Divide by 2
    }

    const bool searchBestMoveForWhite = (this->board.getActiveColor() == chess::Board::WHITE);
    
    // --- ITERATIVE DEEPENING with ASPIRATION WINDOWS ---
    // Search at increasing depths (1, 2, 3, ..., depth)
    // Improve move ordering for subsequent depths
    //
    // ASPIRATION WINDOWS 
    // Instead of searching with [-INF, +INF], use a narrow window around
    // the score from the previous iteration. If the search fails (score outside
    // the window), re-search with progressively wider windows.
    // This dramatically reduces the search tree when the score is stable.
    chess::Board::Move bestMove = moves[0];
    int64_t prevScore = 0; // Score from previous iteration
    
    for (uint64_t currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        this->depth = currentDepth;
        
        // Move ordering: porta la best move della iterazione precedente in testa
        if (currentDepth > 1) {
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i] == bestMove) {
                    chess::Board::Move::rotate(moves, i);
                    break;
                }
            }
        }
        
        // =========================================================================
        // ASPIRATION WINDOW SEARCH
        // =========================================================================
        if (currentDepth <= 4) {
            // At low depths, use full window (score is not reliable yet)
            bestMove = this->getBestMove(moves, searchBestMoveForWhite);
        } else {
            // Use aspiration window centered on previous iteration's score
            constexpr int64_t INITIAL_WINDOW = 50; // Start with ±50cp window
            int64_t windowDelta = INITIAL_WINDOW;
            
            int64_t aspAlpha = prevScore - windowDelta;
            int64_t aspBeta  = prevScore + windowDelta;
            
            // Search with narrow window
            bestMove = this->getBestMove(moves, searchBestMoveForWhite, aspAlpha, aspBeta);
            
            // Check if search failed outside the window
            // If eval is outside [aspAlpha, aspBeta], we need to re-search with wider window
            while (this->eval <= aspAlpha || this->eval >= aspBeta) {
                // Widen the window exponentially
                windowDelta *= 2;
                
                // If window is too wide, fall back to full window
                if (windowDelta > 800) {
                    bestMove = this->getBestMove(moves, searchBestMoveForWhite);
                    break;
                }
                
                // Re-search with wider window
                if (this->eval <= aspAlpha) {
                    aspAlpha = prevScore - windowDelta;
                } else {
                    aspBeta = prevScore + windowDelta;
                }
                bestMove = this->getBestMove(moves, searchBestMoveForWhite, aspAlpha, aspBeta);
            }
        }
        
        prevScore = this->eval; // Save score for next iteration's aspiration window
    }
    
    this->depth = depth; // Restore the original depth

    (void)this->board.move(bestMove.from, bestMove.to, 
        isPromotionMove(this->board, bestMove) ? (bestMove.promotionPiece != '\0' ? bestMove.promotionPiece : 'q') : '\0');
    this->updateGameResult();
    this->bestMove = bestMove;

    this->moveHistory += bestMove.from.toString() + bestMove.to.toString();
    this->moveHistory += bestMove.promotionPiece == '\0' ? "\n" : std::string(1, bestMove.promotionPiece) + "\n";

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
