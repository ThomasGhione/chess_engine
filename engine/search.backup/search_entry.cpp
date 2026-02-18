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
    int64_t alpha = NEG_INF;
    int64_t beta  = POS_INF;
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
                if (score > alpha && score < beta) {
                    score = this->searchPosition(this->board, this->depth - 1, alpha, beta, currPly);
                }
            }

            // Undo move before processing next one
            this->board.undoMove(m, state);

            // Update best move and alpha-beta bounds
            this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);

            // Alpha-beta cutoff
            if (alpha >= beta) break;
        }
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

    // Increment TT generation to invalidate old entries from previous searches
    // This ensures deterministic behavior across multiple searches
    // incrementTTGeneration();

    MoveList<chess::Board::Move> moves = this->generateLegalMoves(this->board);
    if (moves.is_empty()) {
        this->eval = this->evaluate(this->board);
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
    // Aumenta la profondità di ricerca negli endgame
    // Usando flag per garantire che l'aumento sia applicato UNA SOLA VOLTA nella partita
    const int totalPieces = __builtin_popcountll(this->board.getPiecesBitMap());
    
    if (totalPieces == 3 && !this->depthExtendedMaximum) {
        // 2 re + 1 pezzo (K+P vs K, K+Q vs K, etc.)
        depth += 2;
        this->depthExtendedMaximum = true;
    } else if (totalPieces < 6 && !this->depthExtendedMedium) {
        // Endgame con pochi pezzi (es: K+R+P vs K+P)
        depth += 2;
        this->depthExtendedMedium = true;
    }

    const bool searchBestMoveForWhite = (this->board.getActiveColor() == chess::Board::WHITE);
    
    // --- ITERATIVE DEEPENING ---
    // Cerca a profondità crescenti (1, 2, 3, ..., depth)
    // Migliora il move ordering per le profondità successive
    chess::Board::Move bestMove = moves[0];
    
    for (uint64_t currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        this->depth = currentDepth;
        
        // Move ordering: porta la best move della iterazione precedente in testa
        // Usa rotate custom ottimizzata per preservare l'ordinamento relativo
        // CRITICAL: std::swap would break ordering! (the 2nd-best would end up at the i-th position)
        if (currentDepth > 1) {
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i] == bestMove) {
                    // Rotate custom: [A,B,C,D*,E] -> [D*,A,B,C,E] Ordinamento preservato
                    // (invece di swap: [D*,B,C,A,E]  A va in posizione sbagliata!)
                    chess::Board::Move::rotate(moves, i);
                    break;
                }
            }
        }
        
        bestMove = this->getBestMove(moves, searchBestMoveForWhite);
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
