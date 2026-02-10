#include "engine.hpp"
#include "tt.hpp"
#include <atomic>

namespace engine {
    
// Helper for LMR: check if the move is a killer move for the current ply
// OPTIMIZATION: manually unrolled the loop (2 iterations)
__attribute__((always_inline))
inline bool Engine::isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const noexcept {
    if (ply < 0 || ply >= Engine::MAX_PLY) [[unlikely]] return false;
    
    // Manual unroll: compare both killer moves without a loop
    const auto& km0 = killerMoves[0][ply];
    const auto& km1 = killerMoves[1][ply];
    
    return (m.from.index == km0.from.index && m.to.index == km0.to.index) ||
           (m.from.index == km1.from.index && m.to.index == km1.to.index);
}

// Helper function to check if a move is a pawn promotion candidate
// OPTIMIZATION: inline + noexcept + constexpr rank check
__attribute__((always_inline))
inline bool isPromotionMove(const chess::Board& board, const chess::Board::Move& move) noexcept {
    // Early exit: if not on rank 1 or 8, it cannot be a promotion
    const uint8_t toRank = move.to.rank();
    if (toRank != 0 && toRank != 7) return false;
    
    const uint8_t piece = board.get(move.from);
    const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
    
    if (pieceType != chess::Board::PAWN) return false;
    
    const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;
    // White promotes at rank 7 (8th rank), Black promotes at rank 0 (1st rank)
    return toRank == chess::Board::promotionRank(pieceColor == chess::Board::WHITE);
}

// ============================================================================
// MOVE EXECUTION HELPERS - Eliminates doMove/undoMove duplication
// ============================================================================

// Execute a move with automatic promotion detection
// Returns true if the move was a promotion (for information)
__attribute__((always_inline))
inline bool doMoveWithPromotion(chess::Board& b, const chess::Board::Move& m, chess::Board::MoveState& state) noexcept {
    const bool isPromo = isPromotionMove(b, m);
    b.doMove(m, state, isPromo ? 'q' : '\0');
    return isPromo;
}

// Helper to handle terminal nodes and transposition table lookups
bool Engine::handleSearchPrelude(const int64_t& depth, const AlphaBeta& bounds, int64_t& score, uint64_t hashKey) noexcept {
    // REMOVED: Direct evaluate() call at depth<=0
    // Now handled by quiescenceSearch() in searchPosition()
    // This eliminates horizon effect and tactical blunders

    // Transposition table lookup (hashKey already computed by caller to avoid duplication)
    // Prefetch TT only if deep enough to justify overhead
    // depth >= 2: balanced (avoids overhead on shallow/qsearch nodes)
    // Empirical tests show ~5% speedup vs depth >= 0 or depth >= 3
    if (depth >= 2) this->tt.prefetch(hashKey);
    

    return this->tt.probe(hashKey, static_cast<uint8_t>(depth), bounds.alpha, bounds.beta, score);
}

// Helper to search through all moves and find best move with its score
Engine::ScoredMove Engine::searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                                       bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds, bool allowUpdates, bool allowTTWrite) noexcept {
    int64_t best = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = orderedScoredMoves[0].move;

    int moveIndex = 0;
    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        chess::Board::MoveState state;

        const bool wasCapture = (b.get(m.to) != chess::Board::EMPTY);
        const bool isPromo = doMoveWithPromotion(b, m, state);

        // Compute whether the move gives check AFTER it is made
        const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);
        const bool givesCheck = b.inCheck(oppColor);

        // LMR: reduce depth for late, non-critical moves
        // BALANCED: slight improvement in tactics without major speed penalty
        const int64_t childDepth = ctx.depth - 1;
        const bool canReduce = (ctx.depth > 2)               // only reduce if depth > 2...
            && (moveIndex > 6)                               // ...first 6 moves at full depth (compromise)
            && !isPromo                                      // ...isn't a promotion...
            && !wasCapture                                   // ...isn't a capture...
            && !givesCheck                                   // ...doesn't give check...
            && !this->isKillerMove(m, killerMoves, ctx.ply); // ...isn't a killer move

        int64_t score = 0;
        if (canReduce) {
            // Adaptive reduction: balanced between speed and accuracy
            int64_t reduction = 1;
            if (ctx.depth >= 6) reduction += 2; // -2 if depth >= 6
            if (moveIndex >= 10) reduction += 2; // -2 if very late (>= 10th move)
            

            const int64_t reducedDepth = std::max(static_cast<int64_t>(1), childDepth - reduction);
            score = this->searchPosition(b, reducedDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite);
            
            // Re-search at full depth if the reduced search looks promising
            if (score > bounds.alpha && score < bounds.beta) {
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite);
            }
        } else {
            score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite);
        }

        b.undoMove(m, state);

        this->updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

        // Beta cutoff: update killer moves and history, then break
        if (bounds.alpha >= bounds.beta) {
            if (allowUpdates) {
                this->updateKillerAndHistoryOnBetaCutoff(b, m, ctx.depth, ctx.ply, ctx.activeColor,
                                                      bounds.alpha, bounds.beta, history, killerMoves, ctx.previousMove);
            }
            break;
        }
        ++moveIndex;
    }

    return ScoredMove{bestMove, best};
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

// OVERLOAD ottimizzato: implementazione diretta invece di delegare
__attribute__((always_inline))
inline void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& best) noexcept {
    // Update best score if this is better
    if (Engine::isBetter(score, best, usIsWhite)) {
        best = score;
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
#ifdef DEBUG
        std::cout << "[ENDGAME] Depth extended +2 (3 pieces, new depth: " << depth << ")\n";
#endif
    } else if (totalPieces < 6 && !this->depthExtendedMedium) {
        // Endgame con pochi pezzi (es: K+R+P vs K+P)
        depth += 2;
        this->depthExtendedMedium = true;
#ifdef DEBUG
        std::cout << "[ENDGAME] Depth extended +2 (<6 pieces, new depth: " << depth << ")\n";
#endif
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

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply, bool useTT, bool allowTTWrite) noexcept {
    this->nodesSearched++;

    // SAFETY CHECK: evita stack overflow e accesso fuori bounds a killerMoves/history
    if (ply >= MAX_PLY - 1) {
        return this->evaluate(b);
    }

    // QUIESCENCE SEARCH: when depth <= 0, switch to tactical-only search
    // This eliminates horizon effect by searching all captures/checks/promotions
    if (depth <= 0) {
        return this->quiescenceSearch(b, alpha, beta, ply);
    }

    // =========================================================================
    // DRAW BY REPETITION DETECTION (inside search tree)
    // =========================================================================
    // If the current position has already occurred in the game history, treat it
    // as a draw. This prevents the engine from entering perpetual check or
    // shuffling pieces in winning positions.
    //
    // CONTEMPT: When ahead in material, return a score slightly worse than 0
    // to actively discourage drawing. When behind, a draw is acceptable (return 0).
    // This is similar to Stockfish's "contempt" concept.
    //
    // Only check at ply > 0 (not at root) to avoid interfering with root move selection.
    if (ply > 0 && b.isTwofoldRepetition()) {
        const int64_t matDelta = getMaterialDelta(b);
        // Contempt: penalize draw when we have material advantage
        // White ahead (matDelta > 0): return negative score (bad for white = discourages draw)
        // Black ahead (matDelta < 0): return positive score (bad for black = discourages draw)
        // Even material: return 0 (true draw)
        if (std::abs(matDelta) > STALEMATE_MATERIAL_THRESHOLD) {
            // Scale contempt by material advantage (bigger lead = more contempt)
            // Cap at 200cp to avoid distorting search too much
            const int64_t contempt = std::min(static_cast<int64_t>(200), std::abs(matDelta) / 2);
            return (matDelta > 0) ? -contempt : contempt;
        }
        return 0; // True draw
    }

    // 50-move rule detection inside search tree
    if (b.isFiftyMoveRule()) {
        return 0;
    }

    // REMOVED: Endgame depth extension using static bool - buggy
    // Static booleans were never reset across searches, causing missed extensions
    // Fix: handle depth extension in the main search() or use per-search counters

    // OPTIMIZATION: Use incrementally-maintained hash instead of full recomputation
    // computeHashKey() iterates all 12 bitboards (~200+ cycles). getHash() is O(1).
    const uint64_t hashKey = b.getHash();

    // Prepare search structures
    AlphaBeta bounds{alpha, beta};
    int64_t score = 0;

    // Handle terminal nodes, check extensions, and transposition table lookups
    // BUGFIX: Only probe TT if useTT is true (parallel threads must NOT read shared TT)
    if (useTT && this->handleSearchPrelude(depth, bounds, score, hashKey)) {
        return score;
    }

    const uint8_t activeColor = b.getActiveColor();

    // NOTE: Null Move Pruning è disabilitato
    // Reintrodurre quando avremo hash move e better tactical position detection

    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    MoveList<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.is_empty()) {
        // No legal moves: either checkmate or stalemate
        // activeColor = side that CANNOT move (stalemated side)
        
        if (b.inCheck(activeColor)) {
            // Checkmate: activeColor loses
            return usIsWhite ? NEG_INF : POS_INF;
        } else {
            // Stalemate: draw, but heavily penalize throwing away a win
            const int64_t matDelta = getMaterialDelta(b);
            
            if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) {
                return 0; // Balanced material: true draw
            }
            
            // Use reasonable penalty: worse than losing a Queen (900 cp) but not absurd
            // 5000 cp = 50 pawns = clearly terrible, but won't dominate deep searches
            constexpr int64_t STALEMATE_PENALTY = 5000;
            
            // Return from White's perspective
            // If White ahead (matDelta > 0) and position is stalemate: bad for White
            // If Black ahead (matDelta < 0) and position is stalemate: good for White
            return (matDelta > 0) ? -STALEMATE_PENALTY : STALEMATE_PENALTY;
        }
    }

    // Build search context (previousMove passed from parent call)
    SearchContext ctx{depth, bounds.alpha, bounds.beta, ply, activeColor, nullptr};

    MoveList<ScoredMove> orderedScoredMoves = this->sortLegalMoves(moves, ply, b, usIsWhite, hashKey, ctx.previousMove);
    const int64_t alphaOrig = bounds.alpha;

    // Search through all moves and find best move with score
    // BUGFIX: Propagate allowTTWrite to searchMoves so parallel threads never write TT at any depth
    ScoredMove result = this->searchMoves(b, orderedScoredMoves, usIsWhite, ctx, bounds, useTT, allowTTWrite);
    int64_t best = result.score;

    // Save position to transposition table
    // DETERMINISM: save only if allowTTWrite=true (disabled in parallel threads)
    // Reuse hashKey computed earlier to avoid redundant computation
    if (useTT && allowTTWrite) {
        const auto flag = tt::determineFlag(best, alphaOrig, bounds.beta);
        
        // Encode best move for TT storage
        const uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(
            result.move.from.index, result.move.to.index, result.move.promotionPiece);
        
        this->tt.store(hashKey, static_cast<uint8_t>(depth), best, flag, encodedMove);
    }
    return best;
}

__attribute__((always_inline))
inline void addMovesFromMask_fast(const chess::Board& b, MoveList<chess::Board::Move>& moves, const uint8_t from, 
                                  uint64_t mask, const uint64_t ownOcc, const bool inCheck) noexcept {
    mask &= ~ownOcc;
    if (!mask) [[unlikely]] return; // Early exit se nessuna mossa

    const chess::Coords fromC{from};
    const uint8_t fromPieceType = b.get(from) & chess::Board::MASK_PIECE_TYPE;
    const uint8_t color = b.getActiveColor();
    const int promotionRank = chess::Board::promotionRank(color == chess::Board::WHITE);

    // SEMPRE verifica legalità con canMoveToBB
    // Questo è necessario per gestire correttamente pin e mosse illegali
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        if (b.canMoveToBB(fromC, chess::Coords{to}, inCheck)) {
            // Check if this is a pawn promotion move
            if (fromPieceType == chess::Board::PAWN && chess::Board::rankOf(to) == promotionRank) {
                // CORRECTED: generate all 4 promotion moves (q, r, b, n)
                moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}, 'q'});
                moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}, 'r'});
                moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}, 'b'});
                moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}, 'n'});
            } else {
                moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
            }
        }
    }
}

MoveList<chess::Board::Move>
Engine::generateLegalMoves(const chess::Board& b) const noexcept {
    MoveList<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const int side = chess::Board::colorToIndex(color);
    const bool isWhite = (side == 0);

    const uint64_t occ = b.getPiecesBitMap();

    const uint64_t pawns   = b.pawns_bb[side];
    const uint64_t knights = b.knights_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    const uint64_t rooks   = b.rooks_bb[side];
    const uint64_t queens  = b.queens_bb[side];
    const uint64_t kings   = b.kings_bb[side];

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;
    const bool inCheck = b.inCheck(color);

    // ================= KING =================
    if (!kings) [[unlikely]] return moves; // No king found, return empty move list
    

    const uint8_t from = popLSB(const_cast<uint64_t&>(kings));
    const chess::Coords fromC{from};

    // King moves MUST always check legality (can't move to attacked squares)
    uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;
    while (mask) {
        const uint8_t to = popLSB(mask);
        if (b.canMoveToBB(fromC, chess::Coords{to}, inCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }

    // Castling: always needs legality check
    const uint8_t f = from & 7;
    if (f <= 5 && b.canMoveToBB(fromC, chess::Coords{uint8_t(from + 2)}, inCheck))
        moves.emplace_back(chess::Board::Move{fromC, chess::Coords{uint8_t(from + 2)}});
    if (f >= 2 && b.canMoveToBB(fromC, chess::Coords{uint8_t(from - 2)}, inCheck))
        moves.emplace_back(chess::Board::Move{fromC, chess::Coords{uint8_t(from - 2)}});
    

    // NOTE: All generated moves call canMoveToBB to verify legality
    // This ensures no moves that leave the king in check are generated
    // (e.g., moves that violate pins or double-check responses)
    
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::PAWN_ATTACKS[isWhite][from] | pieces::getPawnForwardPushes(from, isWhite, occ);
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getBishopAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getRookAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getQueenAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    return moves;
}

// Helper to add MVV (Most Valuable Victim) bonus for captures
// Simplified from MVV-LVA: only victim matters, attacker is irrelevant (SEE handles exchange eval)
void Engine::addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) noexcept {

    const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
    const uint8_t toPieceType   = b.get(m.to)   & chess::Board::MASK_PIECE_TYPE;

    if (toPieceType != chess::Board::EMPTY) {
        score += MVV_TABLE[toPieceType];  // MVV-only: just victim value
        return;
    }

    // En passant (only pawn moving diagonally to empty square)
    if (fromPieceType == chess::Board::PAWN) {
        if (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index)) {
            score += MVV_TABLE[chess::Board::PAWN];
        }
    }
}


// Helper to add promotion bonus
void Engine::addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score) noexcept {
    if (pieceType == chess::Board::PAWN) {
        if (m.to.rank() == chess::Board::promotionRank(usIsWhite)) {
            score += PIECE_VALUES[chess::Board::QUEEN];
        }
    }
}

// Helper to add check bonus
void Engine::addCheckBonus(const chess::Board::Move& m, chess::Board& b, bool usIsWhite, int64_t& score) noexcept {
    chess::Board::MoveState tmpState;
    b.doMove(m, tmpState, isPromotionMove(b, m) ? 'q' : 0);
    if (b.inCheck(!usIsWhite)) {
        score += CHECK_BONUS;
    }
    b.undoMove(m, tmpState);
}

// Helper to add killer move and history heuristic bonuses
void Engine::addKillerAndHistoryBonus(const chess::Board::Move& m, int ply, bool usIsWhite, int64_t& score) noexcept {
    if (ply >= MAX_PLY) return;

    const auto& km1 = killerMoves[0][ply];
    const auto& km2 = killerMoves[1][ply];

    if (m.from.index == km1.from.index && m.to.index == km1.to.index) {
        score += KILLER1_BONUS;
    } else if (m.from.index == km2.from.index && m.to.index == km2.to.index) {
        score += KILLER2_BONUS;
    }

    const int colorIndex = chess::Board::colorBoolToIndex(usIsWhite);
    const int fromIndex = m.from.index;
    const int toIndex = m.to.index;
    score += history[colorIndex][fromIndex][toIndex];
}

// Helper to add king move heuristic bonus/penalty
// NOTE: inCheck precomputed outside the loop to avoid repeated calls
void Engine::addKingMoveBonus(const chess::Board::Move& m, uint8_t pieceType, bool inCheck, int fullMoveClock, int64_t& score) noexcept {
    if (pieceType != chess::Board::KING) return;

    const int fileDelta = std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index));
    const bool isCastling = (fileDelta == 2);

    // Penalizza mosse del re in apertura se non sotto scacco e non arrocco
    if (fullMoveClock < 10 && !inCheck && !isCastling) {
        score -= KING_NON_CASTLING_PENALTY;
    } else if (isCastling) { 
        score += CASTLING_BONUS;
    }
}

// Static Exchange Evaluation (SEE) - Quick version
// Restituisce il guadagno netto materiale della cattura (positivo = buona, negativo = perdente)
// OPTIMIZATION: stop at the first favorable exchange for the passive side (early exit)
int64_t Engine::staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept {
    const uint8_t toSq = m.to.index;
    const uint8_t fromSq = m.from.index;

    // Micro-ottimizzazione SEE: identifichiamo il meno prezioso controllando
    // i tipi di attaccanti in ordine (pawn -> knight -> bishop -> rook -> queen -> king)
    // e calcolando gli attacchi degli slider solo quando strettamente necessario.
    auto getLeastValuableAttackerTo = [&](uint8_t sq, uint64_t occLocal, int sideLocal) noexcept -> uint8_t {
        // Limit piece bitboards to the simulated occupancy so that pieces
        // that were "captured" in the simulated exchange aren't considered
        // as attackers in subsequent steps.
        const uint64_t pawns_bb = b.pawns_bb[sideLocal] & occLocal;
        const uint64_t knights_bb = b.knights_bb[sideLocal] & occLocal;
        const uint64_t bishops_queens_bb = (b.bishops_bb[sideLocal] | b.queens_bb[sideLocal]) & occLocal;
        const uint64_t rooks_queens_bb = (b.rooks_bb[sideLocal] | b.queens_bb[sideLocal]) & occLocal;
        const uint64_t kings_bb = b.kings_bb[sideLocal] & occLocal;

        uint64_t bb;

        // Pawns
        bb = pawns_bb & pieces::PAWN_ATTACKERS_TO[sideLocal][sq];
        if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

        // Knights
        bb = knights_bb & pieces::KNIGHT_ATTACKS[sq];
        if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

        // Bishops/Queens (diagonal) - compute bishop attacks only now
        bb = bishops_queens_bb & pieces::getBishopAttacks(sq, occLocal);
        if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

        // Rooks/Queens (orthogonal) - compute rook attacks only if needed
        bb = rooks_queens_bb & pieces::getRookAttacks(sq, occLocal);
        if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

        // Kings (last)
        bb = kings_bb & pieces::KING_ATTACKS[sq];
        if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

        return 64; // nessun attaccante
    };

    const int sideActive = b.getActiveColor() == chess::Board::WHITE ? 0 : 1;
    const int sidePassive = sideActive ^ 1;

    // Valore del pezzo catturato inizialmente
    uint8_t capturedType = b.get(toSq) & chess::Board::MASK_PIECE_TYPE;
    if (capturedType == chess::Board::EMPTY) {
        // En passant: cattura un pedone
        capturedType = chess::Board::PAWN;
    }

    // Canonical SEE (swap algorithm):
    // gain[0] = value(victim)
    // for each recapture i:
    //   gain[i] = value(captured_piece) - gain[i-1]
    // where captured_piece is the piece that just moved to the target square in the previous ply.
    constexpr int MAX_SEE_DEPTH = 16;
    int64_t gain[MAX_SEE_DEPTH];
    gain[0] = PIECE_VALUES[capturedType];

    // Simula scambio su occupazione locale
    uint64_t occ = b.getPiecesBitMap();
    occ ^= chess::Board::bitMask(fromSq); // rimuovi il pezzo che fa la prima cattura dalla sua casa

    // Il pezzo ora “in presa” sulla casa target, dopo la mossa iniziale, è il nostro attaccante iniziale.
    uint8_t capturedOnTargetType = b.get(fromSq) & chess::Board::MASK_PIECE_TYPE;

    int depth = 1;
    int side = sidePassive; // il prossimo a catturare è l'avversario

    // EARLY-EXIT: Solo per catture OVVIAMENTE perdenti (es. QxP con riconquista garantita)
    // Soglia CONSERVATIVA: solo se vittima + 400cp < attaccante
    // Questo salta il calcolo SEE solo per catture chiaramente cattive (es. QxP, QxN)
    // ma calcola SEE completo per QxR, RxP, etc. che potrebbero essere buone.
    // FIX BUG: soglia precedente (-200cp) marcava QxR come perdente quando è +400!
    if (PIECE_VALUES[capturedType] + 400 < PIECE_VALUES[capturedOnTargetType]) {
        // Esempio: QxP → 100 + 400 < 900 → TRUE (skip SEE, ritorna -800)
        // Esempio: QxR → 500 + 400 < 900 → FALSE (calcola SEE completo!)
        // Esempio: RxP → 100 + 400 < 500 → FALSE (calcola SEE)
        return static_cast<int64_t>(PIECE_VALUES[capturedType] - PIECE_VALUES[capturedOnTargetType]);
    }

    while (depth < MAX_SEE_DEPTH) {
        // Trova l'attaccante meno prezioso verso la casella target
        uint8_t attacker = getLeastValuableAttackerTo(toSq, occ, side);
        if (attacker == 64) break;

        // Determine attacker type using the piece bitboards AND the simulated occupancy
        // (safer than querying b.get(...) which reflects the original board only).
        const uint64_t attackerMask = chess::Board::bitMask(attacker);
        uint8_t currentAttackerType = chess::Board::PAWN; // default/fallback
        if ((b.pawns_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::PAWN;
        else if ((b.knights_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::KNIGHT;
        else if ((b.bishops_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::BISHOP;
        else if ((b.rooks_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::ROOK;
        else if ((b.queens_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::QUEEN;
        else if ((b.kings_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::KING;

        // In questo ply si cattura il pezzo che era rimasto sulla casa target
        // (cioè il pezzo dell'ultimo catturante).
        gain[depth] = PIECE_VALUES[capturedOnTargetType] - gain[depth - 1];

        // Rimuovi l'attaccante dall'occupancy
        occ ^= attackerMask;

        // Ora sulla casa target rimane il pezzo che ha appena catturato: sarà lui
        // a poter essere catturato nel ply successivo.
        capturedOnTargetType = currentAttackerType;

        // Cambia lato
        side ^= 1;
        depth++;
    }

    // Negamax: propaga il miglior risultato all'indietro
    while (--depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }

    return gain[0];
}


MoveList<Engine::ScoredMove> Engine::sortLegalMoves(
    const MoveList<chess::Board::Move>& moves,
    int ply,
    chess::Board& b,
    bool usIsWhite,
    uint64_t hashKey,
    const chess::Board::Move* previousMove) noexcept
{
    MoveList<ScoredMove> orderedScoredMoves;

    // Pre-calcolo variabili costose fuori dal loop
    const bool inCheck = b.inCheck(b.getActiveColor());
    const int fullMoveClock = b.getFullMoveClock();

    // HASH MOVE: Retrieve from TT for highest priority
    uint16_t encodedHashMove = 0;
    uint8_t hashFrom = 64, hashTo = 64;
    char hashPromo = '\0';
    bool hashMoveIsLegal = false;
    
    // Probe TT to get hash move (don't care about score, just the move)
    int64_t dummyScore = 0;
    this->tt.probe(hashKey, 0, NEG_INF, POS_INF, dummyScore, encodedHashMove);
    
    if (encodedHashMove != 0) {
        tt::TranspositionTable::Entry::decodeMove(encodedHashMove, hashFrom, hashTo, hashPromo);
        
        // Validate hash move is in legal moves list (guards against TT collisions)
        for (const auto& m : moves) {
            if (m.from.index == hashFrom && m.to.index == hashTo && m.promotionPiece == hashPromo) {
                hashMoveIsLegal = true;
                break;
            }
        }
    }

    int moveIndex = 0; // Track move count for lazy check detection
    for (const auto& m : moves) {
        int64_t score = 0;

        const uint8_t fromPiece = b.get(m.from);
        const uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;

        const uint8_t toPiece = b.get(m.to);
        const uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isCapture = (toPieceType != chess::Board::EMPTY);

        // =========================================================
        // MOVE ORDERING PRIORITY (from highest to lowest):
        // 1. Hash move (from TT) → 100000
        // 2. Good captures (SEE >= 0) → 10000 + MVV (1000-9000)
        // 3. Killer move 1 → 9000
        // 4. Killer move 2 → 8500
        // 5. Counter-move (response to prev move) → 8200
        // 6. Checks (non-capture, lazy: first 8 moves) → 8000
        // 7. Promotions (non-capture) → 7000
        // 8. History heuristic → 0-2000
        // 9. Bad captures (SEE < 0) → -10000 + SEE
        // =========================================================

        // Check if this is the hash move (highest priority!)
        // Only assign high priority if hash move was validated as legal
        if (hashMoveIsLegal && m.from.index == hashFrom && m.to.index == hashTo && m.promotionPiece == hashPromo) {
            score = 100000; // Highest priority
        } else if (isCapture) {
            // CAPTURES: priorità basata su SEE + capture history
            const int64_t see = staticExchangeEvaluation(b, m);
            
            if (see >= 0) {
                // GOOD CAPTURE: alta priorità (prima di killer/quiet)
                score = 10000;
                addMVVLVABonus(m, b, score); // +MVV (1000-9000)
                
                // Add capture history bonus (0-500 range)
                const int colorIndex = chess::Board::colorBoolToIndex(usIsWhite);
                const int64_t capHist = captureHistory[colorIndex][m.to.index][toPieceType];
                score += std::min(static_cast<int64_t>(500), capHist / 20); // Scale down
                // Total: 10000-19500
            } else {
                // BAD CAPTURE: low priority, ordered by SEE value
                // Simpler single-tier approach: all bad captures get -10000 + SEE
                score = -10000 + see;
                // Total: -10000 to -10001+ (worse SEE = lower priority)
            }
        } else {
            // NON-CAPTURES: killer, checks, history
            
            // Check for killer moves FIRST (alta priorità)
            bool isKiller = false;
            bool isCounterMove = false;
            if (ply >= 0 && ply < MAX_PLY) {
                const auto& km1 = killerMoves[0][ply];
                const auto& km2 = killerMoves[1][ply];

                if (m.from.index == km1.from.index && m.to.index == km1.to.index) {
                    score = 9000;
                    isKiller = true;
                } else if (m.from.index == km2.from.index && m.to.index == km2.to.index) {
                    score = 8500;
                    isKiller = true;
                }
            }

            // Check for counter-move (response to opponent's previous move)
            if (!isKiller && previousMove != nullptr && previousMove->from.index < 64) {
                const auto& counter = counterMoves[previousMove->from.index][previousMove->to.index];
                if (counter.from.index < 64 && m.from.index == counter.from.index && m.to.index == counter.to.index) {
                    score = 8200; // Between killer moves and checks
                    isCounterMove = true;
                }
            }

            // Se non è killer o counter-move, controlla altre eurystiche
            if (!isKiller && !isCounterMove) {
                // LAZY CHECK DETECTION: only for first 8 non-capture moves
                // Balances tactical strength with performance overhead
                if (moveIndex < 8) {
                    chess::Board::MoveState tmpState;
                    const bool isPromo = isPromotionMove(b, m);
                    b.doMove(m, tmpState, isPromo ? 'q' : '\0');
                    const bool givesCheck = b.inCheck(b.getActiveColor());
                    b.undoMove(m, tmpState);
                    
                    if (givesCheck) {
                        score = 8000; // High priority for checking moves
                    }
                }
                
                // Promotion bonus (se non è cattura e non dà scacco già rilevato)
                if (score == 0 && fromPieceType == chess::Board::PAWN) {
                    if (m.to.rank() == chess::Board::promotionRank(usIsWhite)) {
                        score = 7000;
                    }
                }

                // Discourage placing a bishop directly in front of own pawn (blocks pawn advance)
                if (fromPieceType == chess::Board::BISHOP) {
                    const int toIdx = m.to.index;
                    const int behind = usIsWhite ? (toIdx - 8) : (toIdx + 8);
                    if (behind >= 0 && behind < 64) {
                        const uint64_t pawnMask = usIsWhite ? b.pawns_bb[0] : b.pawns_bb[1];
                        if (pawnMask & chess::Board::bitMask(behind)) {
                            score += -80;
                        }
                    }
                }
                
                // History heuristic (per quiet moves normali)
                if (score == 0 && ply >= 0 && ply < MAX_PLY) {
                    const int colorIndex = chess::Board::colorBoolToIndex(usIsWhite);
                    int64_t histScore = history[colorIndex][m.from.index][m.to.index];
                    // Increased range: 0-2000 (was 0-1000) for better move differentiation
                    score = std::min(static_cast<int64_t>(2000), std::max(static_cast<int64_t>(0), histScore));
                }
                // Discourage moving the same pawn twice in the opening: small negative ordering penalty
                // Simple heuristic: if the pawn is not on its starting rank in the opening, it's likely a second move
                if (fromPieceType == chess::Board::PAWN && fullMoveClock < 8) {
                    const int fromRank = chess::Board::rankOf(m.from.index);
                    const int pawnStartRank = usIsWhite ? 6 : 1; // white pawns start on rank index 6, black on 1
                    if (fromRank != pawnStartRank) {
                        score += ORDERING_PENALTY_SAME_PAWN_OPENING; // negative value lowers priority
                    }
                }
            }
        }

        // NOTE: Stalemate check removed from move ordering (too expensive: doMove/undoMove per move!)
        // Stalemate is now handled ONLY in searchPosition() terminal node evaluation
        // This is much faster and still prevents stalemate in winning positions

        // King move penalties (riduci priorità mosse re in opening se non arrocco)
        if (fromPieceType == chess::Board::KING) {
            const int fileDelta = std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index));
            const bool isCastling = (fileDelta == 2);

            if (fullMoveClock < 10 && !inCheck && !isCastling) {
                score -= 500; // penalità moderata
            } else if (isCastling) {
                score += 1000; // bonus arrocco
            }
        }

        orderedScoredMoves.emplace_back(m, score);
        ++moveIndex; // Increment for lazy check detection threshold
    }

    orderedScoredMoves.sort();

    return orderedScoredMoves;
}

// ============================================================================
// QUIESCENCE SEARCH - Eliminates horizon effect
// ============================================================================
// Searches only tactical moves (captures, promotions) to find a quiet position
// This prevents the engine from stopping the search at a position where a capture sequence is ongoing
// Example: if we search to depth 0 during "Queen takes Pawn, Rook takes Queen", we'd evaluate
// as if we won a pawn, when in reality we're about to lose the Queen.
//
// Delta pruning: Skip captures that can't possibly raise alpha (even if the capture succeeds)
// SEE pruning: Skip losing captures (SEE < threshold)
//
// NOTE: We do NOT generate checks (non-capture) as they cause tree explosion
// Most modern engines only search captures + promotions in qsearch
int64_t Engine::quiescenceSearch(chess::Board& b, int64_t alpha, int64_t beta, int ply) noexcept {
    this->nodesSearched++;

    // SAFETY: prevent stack overflow
    if (ply >= MAX_PLY - 1) {
        return this->evaluate(b);
    }

    // Stand-pat: current static evaluation
    // evaluate() returns score from white's perspective (positive = white winning)
    const int64_t standPat = this->evaluate(b);
    
    // ============================================================================
    // DEPTH LIMIT IN QUIESCENCE - Prevent explosion in complex tactical positions
    // ============================================================================
    // INCREASED: Allow deeper tactical search for better combination vision
    constexpr uint8_t MAX_QSEARCH_DEPTH = 20;
    if (ply >= MAX_QSEARCH_DEPTH) {
        return standPat; // Cutoff profondità - return stand-pat to avoid re-evaluation
    }
    
    const uint8_t activeColor = b.getActiveColor();
    const bool usIsWhite = (activeColor == chess::Board::WHITE);

    // Beta cutoff: position is too good for the active player
    if (isBetaCutoff(standPat, alpha, beta, usIsWhite)) {
        // Early cutoff - don't store in TT (too shallow, overhead not worth it)
        return cutoffValue(alpha, beta, usIsWhite);
    }

    // Update alpha/beta with stand-pat score
    updateBound(standPat, alpha, beta, usIsWhite);

    // ============================================================================
    // EARLY DELTA PRUNING - BEFORE move generation
    // ============================================================================
    // If stand-pat is so bad that even the best possible capture (Queen = 900cp)
    // plus a huge margin can't reach alpha/beta, skip move generation entirely.
    // This saves significant time by avoiding generateTacticalMoves() in hopeless positions.
    // IMPORTANT: Skip this pruning if we're in check (must search all evasions)
    // LESS AGGRESSIVE: increased margin to avoid missing tactical tricks
    constexpr int64_t EARLY_DELTA_MARGIN = 1000; // Queen + bigger margin
    
    const bool inCheck = b.inCheck(activeColor);
    
    if (!inCheck) {
        if (usIsWhite) {
            // White to move: if standPat + margin < alpha, no capture can help
            if (standPat + EARLY_DELTA_MARGIN < alpha) {
                // Early pruning - don't store in TT (too frequent, overhead not worth it)
                return alpha; // Early delta cutoff
            }
        } else {
            // Black to move: if standPat - margin > beta, no capture can help
            if (standPat - EARLY_DELTA_MARGIN > beta) {
                // Early pruning - don't store in TT (too frequent, overhead not worth it)
                return beta; // Early delta cutoff
            }
        }
    }

    // ============================================================================
    // DYNAMIC DELTA PRUNING - Advanced version
    // ============================================================================
    // Delta pruning: if even the best possible capture can't improve our position enough
    // to affect the search result, we can prune the entire qsearch subtree.
    //
    // Dynamic factors:
    // 1. Base margin: Queen value (biggest possible single capture)
    // 2. Promotion bonus: if we have pawns close to promotion
    // 3. Material deficit bonus: if we're losing, we need comebacks (larger delta)
    // 4. Depth penalty: deeper in qsearch = more conservative (reduce delta)
    
    // Compute dynamic delta margin
    int64_t deltaMargin = QUEEN_VALUE; // Base: best single capture
    
    // Factor 1: Check for near-promotion pawns (7th/2nd rank)
    const int side = chess::Board::colorToIndex(activeColor);
    const uint64_t ourPawns = b.pawns_bb[side];
    const uint64_t nearPromoPawns = usIsWhite 
        ? (ourPawns & 0x00FF000000000000ULL) // Rank 7 for white
        : (ourPawns & 0x000000000000FF00ULL); // Rank 2 for black
    
    if (nearPromoPawns) {
        // Add promotion potential to delta (Queen - Pawn = extra 800cp upside)
        deltaMargin += (QUEEN_VALUE - PAWN_VALUE);
    }
    
    // Factor 2: Material deficit - if we're losing, allow more speculative lines
    // Use standPat as proxy for material balance
    const int64_t materialBalance = usIsWhite ? standPat : -standPat;
    if (materialBalance < -300) {
        // Losing by 3+ pawns: add 200cp to delta (need comebacks)
        deltaMargin += 200;
    } else if (materialBalance < -150) {
        // Losing by 1.5 pawns: add 100cp
        deltaMargin += 100;
    }
    
    // Factor 3: Depth penalty - deeper in qsearch = more conservative
    // Reduce delta by 50cp per 5 plies to limit explosion
    const int qsearchDepth = ply - this->depth; // Approximate qsearch depth
    if (qsearchDepth > 5) {
        deltaMargin -= 50 * ((qsearchDepth - 5) / 5);
        deltaMargin = std::max(deltaMargin, static_cast<int64_t>(QUEEN_VALUE)); // Floor at Queen value
    }
    
    // Apply delta pruning with dynamic margin
    if (shouldDeltaPrune(standPat, deltaMargin, alpha, beta, usIsWhite)) {
        return cutoffValue(alpha, beta, usIsWhite);
    }

    // Generate tactical moves (captures, promotions) with checks for pawn tactics
    // Include checks to discover check-based pawn sacrifices and tactics
    MoveList<chess::Board::Move> tacticalMoves = this->generateTacticalMoves(b, true);
    
    // No tactical moves: return stand-pat (quiet position reached)
    if (tacticalMoves.is_empty()) {
        return standPat;
    }

    // Sort tactical moves by MVV-LVA and SEE
    MoveList<ScoredMove> orderedMoves;
    
    // Dynamic SEE pruning threshold based on depth and material balance
    // LESS AGGRESSIVE: allow more slightly-losing captures to find tactics
    // Shallow qsearch (ply < 20): allow losing captures up to -16cp (was -15cp)
    // Deep qsearch (ply >= 20): allow slightly losing captures up to 0cp
    int64_t seeThreshold = (ply < 20) ? -16 : 0;
    
    for (const auto& m : tacticalMoves) {
        const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
        const bool isCapture = (toPieceType != chess::Board::EMPTY);
        
        int64_t score = 0;
        
        if (isCapture) {
            // TODO test this better!!
            // ============================================================================
            // FUTILITY PRUNING IN QSEARCH
            // ============================================================================
            // Skip captures that can't possibly raise alpha, even if they win material.
            // This is aggressive pruning based on material value alone.
            const int64_t capturedValue = PIECE_VALUES[toPieceType];
            // Increase futility margin to be more conservative about skipping captures
            constexpr int64_t FUTILITY_MARGIN = 300; // Safety margin for positional compensation
            
            // Check if this capture can possibly improve our position enough
            if (usIsWhite) {
                // White to move: if standPat + capturedValue + margin < alpha, skip
                if (standPat + capturedValue + FUTILITY_MARGIN < alpha) {
                    continue; // Futility pruning
                }
            } else {
                // Black to move: if standPat - capturedValue - margin > beta, skip
                if (standPat - capturedValue - FUTILITY_MARGIN > beta) {
                    continue; // Futility pruning
                }
            }
            
            const int64_t see = staticExchangeEvaluation(b, m);

            // SEE-based pruning with dynamic threshold
            // Dynamic threshold is already strict enough (-16cp shallow, 0cp deep)
            // No need for additional hard cutoff (was redundant and too permissive at -300cp)
            if (see < seeThreshold) {
                continue;
            }
            
            // PER-MOVE DELTA PRUNING: prune captures that can't improve position
            // Even if this capture is "good" by SEE, if standPat + captureValue + margin
            // still can't reach alpha/beta, skip it
            constexpr int64_t MOVE_DELTA_MARGIN = 200; // Safety margin for positional gains
            
            if (shouldDeltaPrune(standPat, see + MOVE_DELTA_MARGIN, alpha, beta, usIsWhite)) {
                continue; // Per-move delta pruning
            }
            
            // Score by MVV + SEE for better ordering
            // SEE-based ordering: captures with better SEE explored first
            score = 10000 + see; // Base + SEE value (can be negative for losing captures)
            addMVVLVABonus(m, b, score); // Add MVV bonus on top
            // Total: 10000 + see + MVV (1000-9000)
        } else {
            // Non-capture: must be a promotion
            const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
            if (fromPieceType == chess::Board::PAWN && m.to.rank() == chess::Board::promotionRank(usIsWhite)) {
                score = 9000; // Promotion (high priority)
            } else {
                continue;
            }
        }
        
        orderedMoves.emplace_back(m, score);
    }
    
    // If all captures were pruned, return stand-pat
    if (orderedMoves.is_empty()) {
        return standPat;
    }
    
    orderedMoves.sort();

    // Search tactical moves using MINIMAX (not negamax)
    int64_t best = standPat; // Initialize with stand-pat
    
    for (const auto& scoredMove : orderedMoves) {
        const auto& m = scoredMove.move;
        chess::Board::MoveState state;
        
        doMoveWithPromotion(b, m, state);
        
        // MINIMAX: recursively search with same alpha-beta window
        // The side switches automatically because b.doMove() changes activeColor
        const int64_t score = this->quiescenceSearch(b, alpha, beta, ply + 1);
        
        b.undoMove(m, state);
        
        // Update best score
        if (isBetter(score, best, usIsWhite)) {
            best = score;
        }
        
        // Update alpha bound before checking beta cutoff
        updateBound(score, alpha, beta, usIsWhite);
        
        // Alpha-beta pruning
        if (isBetaCutoff(score, alpha, beta, usIsWhite)) {
            // Beta cutoff - don't store in TT (happens too frequently in qsearch)
            return cutoffValue(alpha, beta, usIsWhite);
        }
    }
    
    return best;
}

// ============================================================================
// GENERATE TACTICAL MOVES - Helper for quiescence search
// ============================================================================
// Generates only moves that are tactically relevant:
// 1. Captures (including en passant)
// 2. Pawn promotions (even non-capturing)
// 3. Checks (optional, controlled by QSEARCH_INCLUDE_CHECKS (TODO))
//
// This is a simplified version of generateLegalMoves() optimized for qsearch
MoveList<chess::Board::Move> Engine::generateTacticalMoves(const chess::Board& b, bool includeChecks) const noexcept {
    MoveList<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const int side = chess::Board::colorToIndex(color);
    const bool isWhite = (side == 0);

    const uint64_t occ = b.getPiecesBitMap();

    const uint64_t pawns   = b.pawns_bb[side];
    const uint64_t knights = b.knights_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    const uint64_t rooks   = b.rooks_bb[side];
    const uint64_t queens  = b.queens_bb[side];
    const uint64_t kings   = b.kings_bb[side];

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;
    
    // Opponent occupancy: all pieces minus our pieces
    const uint64_t oppOcc = occ & ~ownOcc;
    
    const bool inCheck = b.inCheck(color);

    // Helper to add tactical moves (captures, promotions, and optionally checks)
    auto addTacticalMovesFromMask = [&](uint8_t from, uint64_t mask, bool isPawn) {
        const chess::Coords fromC{from};
        
        while (mask) {
            const uint8_t to = __builtin_ctzll(mask);
            mask &= (mask - 1);
            
            const chess::Coords toC{to};
            const bool isCapture = (b.get(toC) != chess::Board::EMPTY);
            const bool isPromotion = isPawn && (toC.rank() == chess::Board::promotionRank(isWhite));
            
            // Only add if it's a capture, promotion, or (if includeChecks) a check
            bool shouldAdd = (isCapture || isPromotion);
            
            if (!shouldAdd && includeChecks) {
                // Check if this move gives check (expensive doMove/undoMove)
                if (b.canMoveToBB(fromC, toC, inCheck)) {
                    chess::Board::MoveState tmpState;
                    const_cast<chess::Board&>(b).doMove({fromC, toC, '\0'}, tmpState, isPawn && isPromotion ? 'q' : '\0');
                    if (const_cast<chess::Board&>(b).inCheck(isWhite ? chess::Board::BLACK : chess::Board::WHITE)) {
                        shouldAdd = true;
                    }
                    const_cast<chess::Board&>(b).undoMove({fromC, toC, '\0'}, tmpState);
                }
            }
            
            if (shouldAdd && b.canMoveToBB(fromC, toC, inCheck)) {
                moves.emplace_back(chess::Board::Move{fromC, toC});
            }
        }
    };

    // ================= PAWNS (captures and promotions) =================
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        
        // Pawn attacks (captures only)
        uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
        
        // Pawn forward pushes (only if promotion rank)
        const uint8_t rank = from / 8;
        const uint8_t promotionRank = isWhite ? 6 : 1; // Rank 7 for white, rank 2 for black (0-indexed)
        if (rank == promotionRank) {
            // Check forward push for promotion
            const int direction = isWhite ? 8 : -8;
            const uint8_t frontSq = from + direction;
            if (!(occ & chess::Board::bitMask(frontSq))) {
                attacks |= chess::Board::bitMask(frontSq);
            }
        }
        
        addTacticalMovesFromMask(from, attacks, true);
    }

    // ================= KNIGHTS (captures only) =================
    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::KNIGHT_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= BISHOPS (captures only) =================
    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getBishopAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= ROOKS (captures only) =================
    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getRookAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= QUEENS (captures only) =================
    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getQueenAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= KING (captures only) =================
    if (kings) {
        const uint8_t from = __builtin_ctzll(kings); // King: no need for poplsb (only one)
        uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    return moves;
}


}; //namespace engine

