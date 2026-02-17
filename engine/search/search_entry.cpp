#include "../engine.hpp"
#include "../tt.hpp"

namespace engine {

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

chess::Board::Move Engine::getBestMove(const MoveList<chess::Board::Move>& moves, bool usIsWhite) noexcept {
    return getBestMove(moves, usIsWhite, NEG_INF, POS_INF);
}

chess::Board::Move Engine::getBestMove(const MoveList<chess::Board::Move>& moves, bool usIsWhite, int64_t alpha, int64_t beta) noexcept {
    int64_t bestScore = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = moves[0];
    constexpr int currPly = 1;

    // Parallelismo YBWC: attivato solo se:
    // - Abbastanza mosse (>= 10) per giustificare overhead threads
    // - Profondità sufficiente (>= DEFAULTDEPTH- 2 ) per beneficio reale
    // - NON in endgame (>= 6 pezzi) dove ricerca sequenziale è più efficiente
    const int totalPieces = __builtin_popcountll(this->board.getPiecesBitMap());
    const bool useYBWC = (moves.size >= 10 && 
                          this->depth >= (Engine::DEFAULTDEPTH - 2) &&
                          totalPieces >= 6);
    
    if (!useYBWC) {
        // Sequential search con PVS (Principal Variation Search)
        // Prima mossa: finestra piena
        // Mosse successive: null window, re-search se fallisce
        
        for (int i = 0; i < moves.size; ++i) {
            const auto& m = moves[i];
            chess::Board::MoveState state;
            
            // OTTIMIZZAZIONE: precalcola isPromo UNA VOLTA
            doMoveWithPromotion(this->board, m, state);
            
            int64_t score;
            if (i == 0) {
                // Prima mossa: cerca con finestra piena (PV node)
                score = this->searchPosition(this->board, this->depth - 1, alpha, beta, currPly);
            } else {
                // Mosse successive: cerca con null window
                int64_t nullAlpha, nullBeta;
                if (usIsWhite) {
                    nullAlpha = alpha;
                    nullBeta = alpha + 1; // Null window per white (maximizer)
                } else {
                    nullAlpha = beta - 1; // Null window per black (minimizer)
                    nullBeta = beta;
                }
                
                score = this->searchPosition(this->board, this->depth - 1, nullAlpha, nullBeta, currPly);
                
                // PVS re-search: se null window fallisce, ri-cerca con finestra piena
                // BUGFIX: Correct PVS re-search condition for minimax (not negamax)
                // White: re-search if score > alpha (null window failed high)
                // Black: re-search if score < beta (null window failed low)
                const bool shouldResearch = usIsWhite ? (score > alpha) : (score < beta);
                if (shouldResearch) {
                    score = this->searchPosition(this->board, this->depth - 1, alpha, beta, currPly);
                }
            }

            // Undo move before processing next one
            this->board.undoMove(m, state);

            // Update best move and alpha-beta bounds
            this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);

            // Beta cutoff check after updateMinMax
            // BUGFIX: Use isBetaCutoff() instead of checking alpha >= beta
            if (isBetaCutoff(bestScore, alpha, beta, usIsWhite)) break;
        }
        // BUGFIX: Store eval for sequential path (was missing, always returned 0)
        this->eval = bestScore;
        return bestMove;
    }

    // --- YBWC Parallel - FIXED per determinismo ---
    // Prima mossa: ricerca completa con finestra piena
    {
        const auto& firstMove = moves[0];
        chess::Board::MoveState state;
        
        doMoveWithPromotion(this->board, firstMove, state);
        
        int64_t score = this->searchPosition(this->board, this->depth - 1, alpha, beta, currPly);
        
        // CRITICAL: Undo BEFORE launching parallel threads to avoid races on copying this->board
        this->board.undoMove(firstMove, state);

        this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, firstMove);
    }

    if (moves.size <= 1) [[unlikely]] return bestMove;

    // CRITICAL FIX: Save original alpha/beta before the parallel loop
    // All threads must see the same window to guarantee determinism
    const int64_t originalAlpha = alpha;
    const int64_t originalBeta = beta;

    // std::vector<int64_t> threadScores(moves.size, Engine::initialBest(usIsWhite));
    std::array<int64_t, 218> threadScores; // 218 = max moves 
    threadScores.fill(Engine::initialBest(usIsWhite));

    // Task-based root parallelism (work-stealing, better load balance)
    // Bound the number of threads to MAX_THREADS and the number of moves
    int candidateThreads = static_cast<int>(moves.size - 1);
    if (candidateThreads < 1) candidateThreads = 1;
    const int threadsToUse = (this->MAX_THREADS < candidateThreads) ? this->MAX_THREADS : candidateThreads;

    if (threadsToUse <= 1) {
        // Sequential fallback (avoid OpenMP overhead)
        for (int i = 1; i < moves.size; ++i) {
            chess::Board threadBoard = this->board;
            const auto m = moves[i]; // copy to avoid referencing container inside tasks
            chess::Board::MoveState state;

            doMoveWithPromotion(threadBoard, m, state);
            int64_t score = this->searchPosition(threadBoard, this->depth - 1, originalAlpha, originalBeta, currPly, false, false);
            threadBoard.undoMove(m, state);

            threadScores[i] = score;
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
                            // Make ONE copy of the board for this task using copy ctor (safe)
                            chess::Board threadBoard = this->board;

                            for (int i = start; i < end; ++i) {
                                const auto m = moves[i]; // local copy
                                chess::Board::MoveState state;

                                doMoveWithPromotion(threadBoard, m, state);
                                int64_t score = this->searchPosition(threadBoard, this->depth - 1, originalAlpha, originalBeta, currPly, false, false);
                                threadBoard.undoMove(m, state);

                                threadScores[i] = score;
                            }
                        } // task
                    }
                } // taskgroup
            } // single
        } // parallel
    }

    // Merge results deterministically (in ordine sequenziale, senza race)
    for (int i = 1; i < moves.size; ++i) {
        const int64_t score = threadScores[i];
        const auto& m = moves[i];
        if (Engine::isBetter(score, bestScore, usIsWhite)) {
            bestScore = score;
            bestMove = m;
        }
        // Update alpha/beta bounds
        updateBound(score, alpha, beta, usIsWhite);
    }

    this->eval = bestScore;
    return bestMove;
}

void Engine::doMoveInBoard(chess::Board::Move bestMove) noexcept {
    // Execute the best move found, handling promotions
    // CORRECTED: use the promotion piece from the move, default to 'q' if promotion but no piece specified
    const bool isPromo = isPromotionMove(this->board, bestMove);
    const char promoPiece = isPromo ? (bestMove.promotionPiece != '\0' ? bestMove.promotionPiece : 'q') : '\0';
    (void)this->board.moveBB(bestMove.from, bestMove.to, promoPiece);
}

void Engine::search(uint64_t depth) noexcept {
    if (depth == 0) return;

    // Increment TT generation to age old entries from previous searches
    // This ensures the replacement policy favors fresh entries
    this->tt.incrementGeneration();

    MoveList<chess::Board::Move> moves = this->generateLegalMoves(this->board);
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
    for (int c = 0; c < 2; ++c) {
        for (int from = 0; from < 64; ++from) {
            for (int to = 0; to < 64; ++to) {
                this->history[c][from][to] >>= 1; // Divide by 2 (fast bit shift)
            }
        }
    }

    // --- ENDGAME DEPTH EXTENSION (UNA VOLTA PER PARTITA) ---
    // Aumenta progressivamente la profondità di ricerca negli endgame
    // Più pezzi vengono rimossi, più la search diventa profonda
    // Usando flag per garantire che ogni aumento sia applicato UNA SOLA VOLTA
    const int totalPieces = __builtin_popcountll(this->board.getPiecesBitMap());
    
    if (totalPieces < 6 && !this->depthExtended) {
        depth += 1;
        this->depthExtended = true;
    }
    // REMOVED: depthExtendedEarly (< 10 pieces, +1 depth)
    // Was too aggressive - triggered too early and slowed down search significantly

    const bool searchBestMoveForWhite = (this->board.getActiveColor() == chess::Board::WHITE);
    
    // --- ITERATIVE DEEPENING with ASPIRATION WINDOWS ---
    // Cerca a profondità crescenti (1, 2, 3, ..., depth)
    // Migliora il move ordering per le profondità successive
    //
    // ASPIRATION WINDOWS (+30-50 ELO):
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
                // CONSERVATIVE: 800cp (was 500). Tactical swings of 600-700cp
                // (e.g. queen sacrifice leading to mate) should still use aspiration
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
    
    // Ripristina la profondità originale
    this->depth = depth;

    this->doMoveInBoard(bestMove);
    this->updateGameResult();
    this->bestMove = bestMove;

    this->moveHistory += bestMove.from.toString() + bestMove.to.toString();
    this->moveHistory += bestMove.promotionPiece == '\0' ? "\n" : std::string(1, bestMove.promotionPiece) + "\n";

#ifdef DEBUG
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    // CORRECTED: include promotion piece in UCI notation if present
    if (bestMove.promotionPiece != '\0') {
        moveStr += bestMove.promotionPiece;
    }
    std::cout << "Engine plays: " << moveStr << " (score: " << this->eval << ")\n";
/*
    std::cout << "[DEBUG] TT probes: " << ttProbes
              << ", hits: " << ttHits
              << "\n";
*/
#endif
}

} // namespace engine
