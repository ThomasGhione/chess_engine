#include "../engine.hpp"
#include "../tt.hpp"

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

        // SEE-based pruning/reduction for BAD captures (e.g., Nxg3 with SEE = -220)
        // If a capture is clearly losing material (SEE < threshold), reduce its search depth.
        // This prevents the engine from finding false "compensation" for piece sacrifices
        // through excessive pawn structure penalties deep in the search tree.
        const bool isBadCapture = wasCapture && (scoredMove.score < -500);  // Bad captures have scores like -500-XXX_PIECE_VALUE
        const bool isOpening = b.getFullMoveClock() < 14; // Consider first 10 moves as opening (adjustable threshold)
        const int nonPawnMajors = __builtin_popcountll(board.knights_bb[0] | board.knights_bb[1] |
                                             board.bishops_bb[0] | board.bishops_bb[1] |
                                             board.rooks_bb[0]   | board.rooks_bb[1]   |
                                             board.queens_bb[0]  | board.queens_bb[1]);
        const bool isEndgame = (nonPawnMajors <= 5 /*PIECE_ENDGAME_THRESHOLD*/);
        
        const bool canReduce = (ctx.depth > 2)               // only reduce if depth > 2...
            && (moveIndex > 6)                               // ...first 6 moves at full depth (compromise)
            && !isPromo                                      // ...isn't a promotion...
            && (!wasCapture|| isBadCapture)                  // ...isn't a (good) capture...
            && !givesCheck                                   // ...doesn't give check...
            && !this->isKillerMove(m, killerMoves, ctx.ply); // ...isn't a killer move

        int64_t score = 0;
        if (canReduce) {
            int64_t reduction = 1;
            if (isOpening) {
                if (moveIndex >= 16) reduction += 2; // -2 if late in opening (after 6th move)
            }
            else if (isEndgame) {
                if (moveIndex >= 16) reduction += 2;
            }
            else {
                if (moveIndex >= 12) reduction += 2; // -2 if very late (>= 10th move)  
            }
        
            // Adaptive reduction: balanced between speed and accuracy
            if (ctx.depth >= 6) reduction += 2; // -2 if depth >= 6
            // Extra reduction for bad captures (SEE-losing): these are rarely good moves
            // A knight capturing a defended pawn (SEE = -220) should be searched shallowly
            if (isBadCapture) reduction += 2;    

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
