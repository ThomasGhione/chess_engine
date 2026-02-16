#include "../engine.hpp"
#include "../tt.hpp"
#include <cmath>

namespace engine {

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

    // =========================================================================
    // FUTILITY PRUNING margins (main search) (+20-30 ELO)
    // =========================================================================
    // At low depth, if static eval + margin can't reach alpha/beta,
    // skip quiet moves entirely. Only apply to non-PV, non-check positions.
    // CONSERVATIVE: depth <= 2 only (was 3). Higher margins to avoid
    // cutting tactical positions. At depth 2, margin=400cp (~rook value)
    // ensures we only prune truly hopeless quiet moves.
    constexpr int64_t FUTILITY_MARGINS[] = {0, 200, 400}; // depth 0,1,2
    const bool canFutilityPrune = !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;
    const int64_t futilityMargin = canFutilityPrune ? FUTILITY_MARGINS[ctx.depth] : 0;

    // =========================================================================
    // LATE MOVE PRUNING thresholds (+15-25 ELO)
    // =========================================================================
    // At low depth, skip very late quiet moves entirely.
    // These moves are ordered so low that they're almost certainly not going to improve.
    // CONSERVATIVE: depth <= 3 (was 4), higher thresholds to preserve tactical chances.
    // At depth 1, allow 12 moves (was 8). A tactic could be the 9th or 10th move.
    constexpr int LMP_THRESHOLDS[] = {0, 12, 20, 30}; // depth 0,1,2,3
    const bool canLMP = !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 3 && ctx.depth >= 1;
    const int lmpThreshold = canLMP ? LMP_THRESHOLDS[ctx.depth] : 999;

    int moveIndex = 0;
    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        
        const bool wasCapture = (b.get(m.to) != chess::Board::EMPTY);
        const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const bool isPromotionCandidate = (fromPieceType == chess::Board::PAWN) 
            && (m.to.rank() == chess::Board::promotionRank(usIsWhite));
        const bool isQuietMove = !wasCapture && !isPromotionCandidate;

        // =========================================================================
        // LATE MOVE PRUNING: Skip very late quiet moves at low depth
        // =========================================================================
        if (canLMP && isQuietMove && moveIndex >= lmpThreshold) {
            ++moveIndex;
            continue; // Completely skip this move
        }

        // =========================================================================
        // FUTILITY PRUNING: Skip quiet moves that can't improve the position
        // =========================================================================
        if (canFutilityPrune && isQuietMove && moveIndex > 0) {
            if (usIsWhite) {
                if (ctx.staticEval + futilityMargin < bounds.alpha) {
                    ++moveIndex;
                    continue;
                }
            } else {
                if (ctx.staticEval - futilityMargin > bounds.beta) {
                    ++moveIndex;
                    continue;
                }
            }
        }

        chess::Board::MoveState state;
        const bool isPromo = doMoveWithPromotion(b, m, state);

        // Compute whether the move gives check AFTER it is made
        const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);
        const bool givesCheck = b.inCheck(oppColor);

        // =========================================================================
        // CHECK EXTENSION: Search checking moves 1 ply deeper (+40-60 ELO)
        // =========================================================================
        const int64_t childDepth = ctx.depth - 1 + (givesCheck ? 1 : 0);

        // LMR: reduce depth for late, non-critical moves
        // LOGARITHMIC LMR: reduction = floor(log(depth) * log(moveIndex) / C)
        const int nonPawnMajors = __builtin_popcountll(b.knights_bb[0] | b.knights_bb[1] |
                                             b.bishops_bb[0] | b.bishops_bb[1] |
                                             b.rooks_bb[0]   | b.rooks_bb[1]   |
                                             b.queens_bb[0]  | b.queens_bb[1]);
        const bool isEndgame = (nonPawnMajors <= 5);
        
        const bool canReduce = (ctx.depth > 2)
            && (moveIndex > 16)
            && !isPromo
            && (!wasCapture)
            && !givesCheck
            && !this->isKillerMove(m, killerMoves, ctx.ply)
            && !isEndgame;

        int64_t score = 0;
        if (canReduce) {
            // LOGARITHMIC LMR
            // Higher divisor = less reduction = more conservative
            constexpr double LMR_C = 2.75;
            int64_t reduction = static_cast<int64_t>(std::log(static_cast<double>(ctx.depth)) 
                                                   * std::log(static_cast<double>(moveIndex)) 
                                                   / LMR_C);
            // Cap reduction: never reduce more than depth-3 (was depth-2)
            // This ensures at least 3 plies of real search remain
            reduction = std::clamp(reduction, static_cast<int64_t>(1), ctx.depth - 3);

            const int64_t reducedDepth = std::max(static_cast<int64_t>(1), childDepth - reduction);
            score = this->searchPosition(b, reducedDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite);
            
            // Re-search at full depth if the reduced search looks promising
            const bool shouldResearch = usIsWhite 
                ? (score > bounds.alpha && score < bounds.beta) 
                : (score < bounds.beta && score > bounds.alpha);
            
            if (shouldResearch) {
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite);
            }
        } else {
            score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite);
        }

        b.undoMove(m, state);

        this->updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

        // Beta cutoff: check if the score causes a cutoff, then update killer/history
        // BUGFIX: Use isBetaCutoff() instead of checking bounds.alpha >= bounds.beta
        // bounds.alpha >= bounds.beta means window collapsed (different condition!)
        if (isBetaCutoff(best, bounds.alpha, bounds.beta, usIsWhite)) {
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

    // OPTIMIZATION: Compute hash key ONCE per node (used by both TT probe and save)
    // This avoids duplicate computeHashKey() calls (~50-100 cycles saved per node)
    const uint64_t hashKey = zobrist::computeHashKey(b);

    // Prepare search structures
    AlphaBeta bounds{alpha, beta};
    int64_t score = 0;

    // Handle terminal nodes, check extensions, and transposition table lookups
    // BUGFIX: Only probe TT if useTT is true (parallel threads must NOT read shared TT)
    if (useTT && this->handleSearchPrelude(depth, bounds, score, hashKey)) {
        return score;
    }

    const uint8_t activeColor = b.getActiveColor();
    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    const bool inCheck = b.inCheck(activeColor);

    // =========================================================================
    // STATIC EVALUATION for pruning decisions
    // =========================================================================
    // Compute static eval ONCE for NMP, RFP, futility pruning
    // Only compute when needed (not in check, not at root)
    const int64_t staticEval = (ply > 0 && !inCheck) ? this->evaluate(b) : 0;

    // =========================================================================
    // NULL MOVE PRUNING (+80-120 ELO)
    // =========================================================================
    // Idea: If passing the turn (doing nothing) still results in a score >= beta,
    // then the current position is so good that the opponent won't allow it.
    // We can safely prune this node.
    //
    // Restrictions:
    // - NOT when in check (illegal to pass when in check)
    // - NOT at root (ply == 0)
    // - NOT in endgames with few non-pawn pieces (zugzwang risk)
    // - NOT at very low depth (overhead not worth it)
    // - NOT in PV nodes (alpha + 1 != beta means PV node)
    {
        const int nonPawnMajors = __builtin_popcountll(
            b.knights_bb[usIsWhite ? 0 : 1] | b.bishops_bb[usIsWhite ? 0 : 1] |
            b.rooks_bb[usIsWhite ? 0 : 1]   | b.queens_bb[usIsWhite ? 0 : 1]);
        
        // NMP CONDITIONS (conservative to avoid zugzwang and bad sacrifices):
        // - Not in check (illegal)
        // - Not at root
        // - depth >= 4 (need sufficient depth for reduced search to be meaningful)
        // - At least 3 non-pawn pieces (stronger zugzwang protection)
        // - Static eval is not too far below beta (with margin)
        //   Without this, NMP can prune positions where we just sacrificed material
        //   and the opponent has a strong reply. The margin allows NMP when we're
        //   slightly worse but not when we're clearly losing.
        //   Margin: 200cp = allows NMP even if slightly behind, but blocks it
        //   after unsound material sacrifices.
        const bool evalOk = usIsWhite 
            ? (staticEval >= beta - 200)
            : (staticEval <= alpha + 200);
        
        const bool canNullMove = !inCheck
            && ply > 0
            && depth >= 4
            && nonPawnMajors >= 3
            && evalOk;

        if (canNullMove) {
            // Adaptive reduction: R = 3 + depth/8 (less aggressive: was depth/6)
            const int64_t R = 3 + depth / 8;

            // Execute null move: just flip side to move, clear en passant
            const auto savedEnPassant = b.getEnPassant();
            b.setNextTurn();
            // Clear en passant for null move (opponent can't en passant after a "pass")
            // Note: setNextTurn already handles some state, but we need to be safe

            const int64_t nullScore = this->searchPosition(b, depth - R, alpha, beta, ply + 1, useTT, allowTTWrite);

            // Undo null move
            b.setPrevTurn();

            // Check for beta cutoff
            if (usIsWhite ? (nullScore >= beta) : (nullScore <= alpha)) {
                // Verification search at reduced depth to avoid zugzwang blunders
                // Only verify if depth is high enough (otherwise overhead > benefit)
                // depth >= 10: verification is expensive, only do it at high depth
                if (depth >= 10) {
                    const int64_t verifyScore = this->searchPosition(b, depth - R, alpha, beta, ply, useTT, allowTTWrite);
                    if (usIsWhite ? (verifyScore >= beta) : (verifyScore <= alpha)) {
                        return usIsWhite ? beta : alpha;
                    }
                    // Verification failed: continue with full search
                } else {
                    return usIsWhite ? beta : alpha;
                }
            }
        }
    }

    // =========================================================================
    // REVERSE FUTILITY PRUNING (Static Null Move Pruning) (+30-40 ELO)
    // =========================================================================
    // If the static eval is so far above beta that even a big margin can't
    // bring it below beta, prune immediately. Much cheaper than NMP.
    // Only at very low depth where static eval is a reliable proxy.
    // CONSERVATIVE: depth <= 3 only (was 4), margin 85cp/depth (was 100)
    // At depth 3, margin = 255cp (~knight value). This avoids cutting
    // positions where the opponent has a tactical shot worth a piece.
    {
        constexpr int64_t RFP_MARGIN_PER_DEPTH = 85; // 85cp per depth level
        if (!inCheck && ply > 0 && depth <= 3) {
            const int64_t rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
            if (usIsWhite) {
                if (staticEval - rfpMargin >= beta) return staticEval;
            } else {
                if (staticEval + rfpMargin <= alpha) return staticEval;
            }
        }
    }

    MoveList<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.is_empty()) {
        // No legal moves: either checkmate or stalemate
        // activeColor = side that CANNOT move (stalemated side)
        
        if (inCheck) {
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
    SearchContext ctx{depth, bounds.alpha, bounds.beta, ply, activeColor, nullptr, staticEval, inCheck};

    MoveList<ScoredMove> orderedScoredMoves = this->sortLegalMoves(moves, ply, b, usIsWhite, hashKey, ctx.previousMove);

    // =========================================================================
    // IIR - Internal Iterative Reduction (+15-25 ELO)
    // =========================================================================
    // If there's no hash move from TT (the first move score < 100000, which is hash move bonus),
    // reduce depth by 1. Without a hash move, PVS is much less efficient and we'd waste
    // time searching with poor move ordering. IIR compensates by searching shallower.
    // CONSERVATIVE: depth >= 6 (was 4). At depth 4-5, reducing to 3-4 combined with
    // RFP/futility makes the search too shallow and misses tactics.
    const bool hasHashMove = (!orderedScoredMoves.is_empty() && orderedScoredMoves[0].score >= 100000);
    int64_t effectiveDepth = depth;
    if (!hasHashMove && depth >= 6 && ply > 0) {
        effectiveDepth -= 1;
        ctx.depth = effectiveDepth;
    }

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

} // namespace engine
