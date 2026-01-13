#include "engine.hpp"
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
    // White promotes at rank 0 (row 8), Black promotes at rank 7 (row 1)
    return (pieceColor == chess::Board::WHITE && toRank == 0) ||
           (pieceColor == chess::Board::BLACK && toRank == 7);
}

// Helper to handle terminal nodes and transposition table lookups
bool Engine::handleSearchPrelude(const chess::Board& b, const int64_t& depth, const AlphaBeta& bounds, int64_t& score, uint64_t hashKey) noexcept {
    
    // const uint8_t activeColor = b.getActiveColor();

    // NOTE: isCheckmate/isStalemate are handled implicitly when generateLegalMoves()
    // returns empty in searchPosition, so explicit checks are not required here.
    if (depth <= 0) {
        score = this->evaluate(b);
        return true;
    }

    // Check extension: search deeper if in check, but limit to avoid explosion
    // Limit extended depth to avoid stack overflow and segfaults
    // constexpr int64_t MAX_EXTENDED_DEPTH = 10;
    // const bool inCheck = b.inCheck(activeColor);
    // if (inCheck && depth > 0 && depth < MAX_EXTENDED_DEPTH) {
    //     depth++;
    // }

    // Transposition table lookup (hashKey already computed by caller to avoid duplication)
    // Prefetch TT only if deep enough to justify overhead
    // depth >= 2: balanced (avoids overhead on shallow/qsearch nodes)
    // Empirical tests show ~5% speedup vs depth >= 0 or depth >= 3
    if (depth >= 2) this->tt.prefetch(hashKey);
    

    return this->tt.probe(hashKey, static_cast<uint8_t>(depth), bounds.alpha, bounds.beta, score);
}

// Helper to search through all moves and find best move with its score
Engine::ScoredMove Engine::searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                                       bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds, bool allowUpdates) noexcept {
    int64_t best = usIsWhite ? NEG_INF : POS_INF;
    chess::Board::Move bestMove = orderedScoredMoves[0].move;

    int moveIndex = 0;
    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        chess::Board::MoveState state;

        const bool wasCapture = (b.get(m.to) != chess::Board::EMPTY);
        const bool isPromo = isPromotionMove(b, m);

        // UNIFIED: use a single doMove call with an optional parameter
        b.doMove(m, state, isPromo ? 'q' : '\0');

        // Compute whether the move gives check AFTER it is made
        const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);
        const bool givesCheck = b.inCheck(oppColor);

        // LMR: reduce depth for late, non-critical moves
        const int64_t childDepth = ctx.depth - 1;
        const bool canReduce = (ctx.depth > 2)               // only reduce if depth > 2...
            && (moveIndex > 4)                               // ...the first 4 moves at full depth
            && !isPromo                                      // ...isn't a promotion...
            && !wasCapture                                   // ...isn't a capture...
            && !givesCheck                                   // ...doesn't give check...
            && !this->isKillerMove(m, killerMoves, ctx.ply); // ...isn't a killer move

        int64_t score = 0;
        if (canReduce) {
            // Adaptive reduction: deeper searches reduce more
            int64_t reduction = 1;
            if (ctx.depth >= 6) reduction += 2; // -2 if depth >= 6
            if (moveIndex >= 8) reduction += 2; // -2 if very late (>= 8th move)
            

            const int64_t reducedDepth = std::max(static_cast<int64_t>(1), childDepth - reduction);
            score = this->searchPosition(b, reducedDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates);
            
            // Re-search at full depth if the reduced search looks promising
            if (score > bounds.alpha && score < bounds.beta) {
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates);
            }
        } else {
            score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates);
        }

        b.undoMove(m, state);

        this->updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

        // Beta cutoff: update killer moves and history, then break
        if (bounds.alpha >= bounds.beta) {
            if (allowUpdates) {
                this->updateKillerAndHistoryOnBetaCutoff(b, m, ctx.depth, ctx.ply, ctx.activeColor,
                                                      bounds.alpha, bounds.beta, history, killerMoves);
            }
            break;
        }
        ++moveIndex;
    }

    return ScoredMove{bestMove, best};
}

void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                          chess::Board::Move& bestMove, const chess::Board::Move& m) noexcept {
    if (usIsWhite) {
        // White is the maximizing player
        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }
        if (score > alpha) alpha = score;
        return;
    }
    // Black is the minimizing player
    if (score < bestScore) {
        bestScore = score;
        bestMove = m;
    }
    if (score < beta) beta = score;
}

// OVERLOAD ottimizzato: implementazione diretta invece di delegare
__attribute__((always_inline))
inline void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& best) noexcept {
    if (usIsWhite) {
        if (score > best) best = score;
        if (score > alpha) alpha = score;
    } else {
        if (score < best) best = score;
        if (score < beta) beta = score;
    }
}

chess::Board::Move Engine::getBestMove(const MoveList<chess::Board::Move>& moves, bool usIsWhite) noexcept {
    int64_t alpha = NEG_INF;
    int64_t beta  = POS_INF;
    int64_t bestScore = usIsWhite ? NEG_INF : POS_INF;
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
            const bool isPromo = isPromotionMove(this->board, m);
            this->board.doMove(m, state, isPromo ? 'q' : '\0');
            
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
            
            this->undoAndUpdateMove(m, state, usIsWhite, score, alpha, beta, bestScore, bestMove);
            
            // Alpha-beta cutoff
            if (alpha >= beta) break;
        }
        return bestMove;
    }

    // --- YBWC Parallel - FIXED per determinismo ---
    // Prima mossa: ricerca completa con finestra piena
    {
        const auto& firstMove = moves[0];
        chess::Board::MoveState state;
        
        const bool isPromo = isPromotionMove(this->board, firstMove);
        this->board.doMove(firstMove, state, isPromo ? 'q' : '\0');
        
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

    std::vector<int64_t> threadScores(moves.size, usIsWhite ? NEG_INF : POS_INF);

    #pragma omp parallel for schedule(static, 1)
    for (int i = 1; i < moves.size; ++i) {
        chess::Board threadBoard = this->board;
        const auto& m = moves[i];
        chess::Board::MoveState state;

        if (isPromotionMove(this->board, m)) [[unlikely]] {
            threadBoard.doMove(m, state, 'q');
        }
        else [[likely]] {
            threadBoard.doMove(m, state);
        }
        
        int64_t score = this->searchPosition(threadBoard, this->depth - 1, originalAlpha, originalBeta, currPly, false, false);
        threadBoard.undoMove(m, state);

        threadScores[i] = score;
    }

    // Merge results deterministically (in ordine sequenziale, senza race)
    for (int i = 1; i < moves.size; ++i) {
        const int64_t score = threadScores[i];
        const auto& m = moves[i];
        if (usIsWhite) {
            if (score > bestScore) { bestScore = score; bestMove = m; }
            if (score > alpha) alpha = score;
        } else {
            if (score < bestScore) { bestScore = score; bestMove = m; }
            if (score < beta) beta = score;
        }
    }

    this->eval = bestScore;
    return bestMove;
}

// REMOVED: executeMove() was redundant - use doMove with promotion check inline
// The optimized version is already implemented in searchMoves() and getBestMove()

// Helper to undo move and update best move/alpha-beta bounds
void Engine::undoAndUpdateMove(const chess::Board::Move& m, const chess::Board::MoveState& state, bool usIsWhite,
                               int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore,
                               chess::Board::Move& bestMove) noexcept {
    // Undo move before processing next one
    this->board.undoMove(m, state);

    // Update best move and alpha-beta bounds
    this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);
}

void Engine::doMoveInBoard(chess::Board::Move bestMove) noexcept {
    // Execute the best move found, handling promotions
    if (isPromotionMove(this->board, bestMove)) [[unlikely]] {
        (void)this->board.moveBB(bestMove.from, bestMove.to, 'q');
        return;
    }
    (void)this->board.moveBB(bestMove.from, bestMove.to);
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
    this->setIsCheckMate();

    this->moveHistory += bestMove.from.toString() + bestMove.to.toString();
    this->moveHistory += bestMove.promotionPiece == '\0' ? "\n" : std::string(1, bestMove.promotionPiece) + "\n";

#ifdef DEBUG
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
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

    // REMOVED: Endgame depth extension using static bool - buggy
    // Static booleans were never reset across searches, causing missed extensions
    // Fix: handle depth extension in the main search() or use per-search counters

    // OPTIMIZATION: Compute hash key ONCE per node (used by both TT probe and save)
    // This avoids duplicate computeHashKey() calls (~50-100 cycles saved per node)
    const uint64_t hashKey = zobrist::computeHashKey(b);

    // Prepare search structures
    AlphaBeta bounds{alpha, beta};
    int64_t score = 0;

    // Handle terminal nodes, check extensions, and transposition table lookups
    if (this->handleSearchPrelude(b, depth, bounds, score, hashKey)) {
        return score;
    }

    const uint8_t activeColor = b.getActiveColor();

    // NOTE: Null Move Pruning è disabilitato
    // Reintrodurre quando avremo hash move e better tactical position detection

    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    MoveList<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.is_empty()) return this->evaluate(b);

    MoveList<ScoredMove> orderedScoredMoves = this->sortLegalMoves(moves, ply, b, usIsWhite);
    const int64_t alphaOrig = bounds.alpha;

    // Build search context
    SearchContext ctx{depth, bounds.alpha, bounds.beta, ply, activeColor};

    // Search through all moves and find best move with score
    ScoredMove result = this->searchMoves(b, orderedScoredMoves, usIsWhite, ctx, bounds, useTT);
    int64_t best = result.score;

    // Save position to transposition table
    // DETERMINISM: save only if allowTTWrite=true (disabled in parallel threads)
    // Reuse hashKey computed earlier to avoid redundant computation
    if (useTT && allowTTWrite) {
        const auto flag = tt::determineFlag(best, alphaOrig, bounds.beta);
        this->tt.store(hashKey, static_cast<uint8_t>(depth), best, flag);
    }
    return best;
}


__attribute__((always_inline))
inline uint8_t poplsb(uint64_t& bb) noexcept {
    const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bb));
    bb &= bb - 1;
    return sq;
}

__attribute__((always_inline))
inline void addMovesFromMask_fast(const chess::Board& b, MoveList<chess::Board::Move>& moves, const uint8_t from, 
                                  uint64_t mask, const uint64_t ownOcc, const bool inCheck) noexcept {
    mask &= ~ownOcc;
    if (!mask) [[unlikely]] return; // Early exit se nessuna mossa

    const chess::Coords fromC{from};

    // SEMPRE verifica legalità con canMoveToBB
    // Questo è necessario per gestire correttamente pin e mosse illegali
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        if (b.canMoveToBB(fromC, chess::Coords{to}, inCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
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
    

    const uint8_t from = poplsb(const_cast<uint64_t&>(kings));
    const chess::Coords fromC{from};

    // King moves MUST always check legality (can't move to attacked squares)
    uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;
    while (mask) {
        const uint8_t to = poplsb(mask);
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
        const uint8_t from = poplsb(bb);
        uint64_t mask = pieces::PAWN_ATTACKS[isWhite][from] | pieces::getPawnForwardPushes(from, isWhite, occ);
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = knights;
    while (bb) {
        const uint8_t from = poplsb(bb);
        uint64_t mask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = bishops;
    while (bb) {
        const uint8_t from = poplsb(bb);
        uint64_t mask = pieces::getBishopAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = rooks;
    while (bb) {
        const uint8_t from = poplsb(bb);
        uint64_t mask = pieces::getRookAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = queens;
    while (bb) {
        const uint8_t from = poplsb(bb);
        uint64_t mask = pieces::getQueenAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    return moves;
}

// Helper to add MVV-LVA bonus for captures
void Engine::addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) noexcept {

    const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
    const uint8_t toPieceType   = b.get(m.to)   & chess::Board::MASK_PIECE_TYPE;

    if (toPieceType != chess::Board::EMPTY) {
        score += MVV_LVA_TABLE[toPieceType][fromPieceType];
        return;
    }

    // En passant (only pawn moving diagonally to empty square)
    if (fromPieceType == chess::Board::PAWN) {
        if ((m.from.index & 7) != (m.to.index & 7)) {
            score += MVV_LVA_TABLE[chess::Board::PAWN][chess::Board::PAWN];
        }
    }
}


// Helper to add promotion bonus
void Engine::addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score) noexcept {
    if (pieceType == chess::Board::PAWN) {
        if ((usIsWhite && m.to.rank() == 7) || (!usIsWhite && m.to.rank() == 0)) {
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

    const int colorIndex = usIsWhite ? 0 : 1;
    const int fromIndex = m.from.index;
    const int toIndex = m.to.index;
    score += history[colorIndex][fromIndex][toIndex];
}

// Helper to add king move heuristic bonus/penalty
// NOTE: inCheck precomputed outside the loop to avoid repeated calls
void Engine::addKingMoveBonus(const chess::Board::Move& m, uint8_t pieceType, bool inCheck, int fullMoveClock, int64_t& score) noexcept {
    if (pieceType != chess::Board::KING) return;

    const int fileDelta = std::abs((m.to.index & 7) - (m.from.index & 7));
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

    // Trova tutti gli attaccanti alla casella target
    auto getAttackersTo = [&](uint8_t sq, uint64_t occ, int side) noexcept -> uint64_t {
        uint64_t attackers = 0ULL;
        
        // Pawns
        attackers |= b.pawns_bb[side] & pieces::PAWN_ATTACKERS_TO[side][sq];
        
        // Knights
        attackers |= b.knights_bb[side] & pieces::KNIGHT_ATTACKS[sq];
        
        // Kings
        attackers |= b.kings_bb[side] & pieces::KING_ATTACKS[sq];
        
        // Bishops and Queens (diagonal)
        uint64_t diagSliders = b.bishops_bb[side] | b.queens_bb[side];
        if (diagSliders) [[likely]] {
            uint64_t bishopAtks = pieces::getBishopAttacks(sq, occ);
            attackers |= diagSliders & bishopAtks;
        }
        
        // Rooks and Queens (orthogonal)
        uint64_t orthSliders = b.rooks_bb[side] | b.queens_bb[side];
        if (orthSliders) [[likely]] {
            uint64_t rookAtks = pieces::getRookAttacks(sq, occ);
            attackers |= orthSliders & rookAtks;
        }
        
        return attackers;
    };

    // Trova l'attaccante meno prezioso
    auto getLeastValuableAttacker = [&](uint64_t attackers, const chess::Board& board) noexcept -> uint8_t {
        // Ordine: pawn, knight, bishop, rook, queen, king
        const uint64_t pieceTypes[6] = {
            attackers & (board.pawns_bb[0] | board.pawns_bb[1]),
            attackers & (board.knights_bb[0] | board.knights_bb[1]),
            attackers & (board.bishops_bb[0] | board.bishops_bb[1]),
            attackers & (board.rooks_bb[0] | board.rooks_bb[1]),
            attackers & (board.queens_bb[0] | board.queens_bb[1]),
            attackers & (board.kings_bb[0] | board.kings_bb[1])
        };
        
        for (uint64_t bb : pieceTypes) {
            if (bb) return __builtin_ctzll(bb);
        }
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

    // Quick SEE: array ridotto da 32 a 16 livelli (praticamente mai raggiunti)
    constexpr int MAX_SEE_DEPTH = 16;
    int64_t gain[MAX_SEE_DEPTH];
    gain[0] = PIECE_VALUES[capturedType];

    // Simula scambio
    uint64_t occ = b.getPiecesBitMap();
    uint64_t fromBB = 1ULL << fromSq;
    occ ^= fromBB; // rimuovi il pezzo che fa la prima cattura
    
    uint8_t attackerType = b.get(fromSq) & chess::Board::MASK_PIECE_TYPE;
    int depth = 1;
    int side = sidePassive; // il prossimo a catturare è l'avversario

    while (depth < MAX_SEE_DEPTH) {
        // Trova attaccanti del lato corrente
        uint64_t attackers = getAttackersTo(toSq, occ, side);
        if (!attackers) break;

        // Prendi l'attaccante meno prezioso
        uint8_t attacker = getLeastValuableAttacker(attackers, b);
        if (attacker == 64) break;

        // Calcola guadagno: catturi il pezzo precedente, perdi quello corrente
        gain[depth] = PIECE_VALUES[attackerType] - gain[depth - 1];
        
        // PRUNING: se il guadagno attuale è già peggiore del miglior risultato possibile,
        // possiamo terminare early (alpha-beta pruning applicato a SEE)
        // NON usiamo early exit basato solo sul lato passivo perché non è corretto in negamax
        
        // Rimuovi l'attaccante dall'occupancy
        occ ^= (1ULL << attacker);
        attackerType = b.get(attacker) & chess::Board::MASK_PIECE_TYPE;
        
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
    bool usIsWhite) noexcept
{
    MoveList<ScoredMove> orderedScoredMoves;

    // Pre-calcolo variabili costose fuori dal loop
    const bool inCheck = b.inCheck(b.getActiveColor());
    const int fullMoveClock = b.getFullMoveClock();

    for (const auto& m : moves) {
        int64_t score = 0;

        const uint8_t fromPiece = b.get(m.from);
        const uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;

        const uint8_t toPiece = b.get(m.to);
        const uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isCapture = (toPieceType != chess::Board::EMPTY);

        // =========================================================
        // MOVE ORDERING PRIORITY (from highest to lowest):
        // 1. Hash move (TODO: not implemented yet) → 100000+
        // 2. Good captures (SEE >= 0) → 10000 + MVV-LVA
        // 3. Killer moves → 9000/8500
        // 4. Checks (non-capture) → 8000
        // 5. Promotions (non-capture) → 7000
        // 6. History heuristic → 0-1000
        // 7. Bad captures (SEE < 0) → -10000 + SEE
        // =========================================================

        if (isCapture) {
            // CAPTURES: priorità basata su SEE
            const int64_t see = staticExchangeEvaluation(b, m);
            
            if (see >= 0) {
                // GOOD CAPTURE: alta priorità (prima di killer/quiet)
                score = 10000;
                addMVVLVABonus(m, b, score); // +MVV-LVA (0-9000)
                // Total: 10000-19000
            } else {
                // BAD CAPTURE: bassa priorità (dopo tutto)
                score = -10000 + see; // see è negativo, quindi score molto basso
                // Total: -10000 a -10500 circa
            }
        } else {
            // NON-CAPTURES: killer, checks, history
            
            // Check for killer moves FIRST (alta priorità)
            bool isKiller = false;
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

            // Se non è killer, controlla altre eurystiche
            if (!isKiller) {
                // OPTIMIZATION: remove check detection from move ordering
                // It is expensive (doMove/undoMove per quiet move)
                // Checks will still be explored early because of killer moves and LMR
                
                // Promotion bonus (se non è cattura)
                if (fromPieceType == chess::Board::PAWN) {
                    if ((usIsWhite && m.to.rank() == 7) || (!usIsWhite && m.to.rank() == 0)) {
                        score = 7000;
                    }
                }
                
                // History heuristic (per quiet moves normali)
                if (score == 0 && ply >= 0 && ply < MAX_PLY) {
                    const int colorIndex = usIsWhite ? 0 : 1;
                    int64_t histScore = history[colorIndex][m.from.index][m.to.index];
                    // Clampiamo a [0, 1000] per evitare valori anomali
                    score = std::min(static_cast<int64_t>(1000), std::max(static_cast<int64_t>(0), histScore));
                }
            }
        }

        // King move penalties (riduci priorità mosse re in opening se non arrocco)
        if (fromPieceType == chess::Board::KING) {
            const int fileDelta = std::abs((m.to.index & 7) - (m.from.index & 7));
            const bool isCastling = (fileDelta == 2);

            if (fullMoveClock < 10 && !inCheck && !isCastling) {
                score -= 500; // penalità moderata
            } else if (isCastling) {
                score += 1000; // bonus arrocco
            }
        }

        orderedScoredMoves.emplace_back(m, score);
    }

    orderedScoredMoves.sort();

    return orderedScoredMoves;
}


}; //namespace engine
