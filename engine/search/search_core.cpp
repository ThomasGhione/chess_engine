#include "../engine.hpp"
#include "../tt.hpp"
#include <cmath>

namespace engine {

int64_t Engine::stalemateScoreFromMaterialDelta(int64_t matDelta) noexcept {
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) {
        return 0;
    }

    // Penalize stalemate when the side with material advantage allows it.
    constexpr int64_t STALEMATE_PENALTY = 5000;
    return (matDelta > 0) ? -STALEMATE_PENALTY : STALEMATE_PENALTY;
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

    // =========================================================================
    // HISTORY MALUS: Track quiet moves searched before cutoff (+10-20 ELO)
    // =========================================================================
    // When a beta cutoff occurs, penalize all quiet moves that were searched
    // but failed to produce a cutoff. This improves move ordering over time.
    struct QuietEntry { uint8_t from; uint8_t to; };
    constexpr int MAX_QUIETS_TRACKED = 64;
    QuietEntry searchedQuiets[MAX_QUIETS_TRACKED];
    int numSearchedQuiets = 0;

    // Pre-compute endgame buckets once before the loop.
    const int nonPawnMajorsForLMR = __builtin_popcountll(b.knights_bb[0] | b.knights_bb[1] |
                                             b.bishops_bb[0] | b.bishops_bb[1] |
                                             b.rooks_bb[0]   | b.rooks_bb[1]   |
                                             b.queens_bb[0]  | b.queens_bb[1]);
    // Delicate endings (pure minor/pawn/king races): keep pruning conservative.
    const bool isDelicateEndgame = (nonPawnMajorsForLMR <= 2);
    // Broad ending bucket used for softer margins/thresholds.
    const bool isLateEndgame = (nonPawnMajorsForLMR <= 5);

    // =========================================================================
    // FUTILITY PRUNING margins (main search) (+20-30 ELO)
    // =========================================================================
    // In late endgames keep pruning active but with tighter margins.
    constexpr int64_t FUTILITY_MARGINS_MG[] = {0, 200, 400}; // depth 0,1,2
    constexpr int64_t FUTILITY_MARGINS_EG[] = {0, 120, 240}; // depth 0,1,2
    const bool canFutilityPrune = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;
    const int64_t futilityMargin = canFutilityPrune
        ? (isLateEndgame ? FUTILITY_MARGINS_EG[ctx.depth] : FUTILITY_MARGINS_MG[ctx.depth])
        : 0;

    // =========================================================================
    // LATE MOVE PRUNING thresholds (+15-25 ELO)
    // =========================================================================
    // In late endgames, prune later (higher threshold) to avoid dropping key pawn moves.
    constexpr int LMP_THRESHOLDS_MG[] = {0, 12, 20, 30}; // depth 0,1,2,3
    constexpr int LMP_THRESHOLDS_EG[] = {0, 16, 26, 38}; // depth 0,1,2,3
    const bool canLMP = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 3 && ctx.depth >= 1;
    const int lmpThreshold = canLMP
        ? (isLateEndgame ? LMP_THRESHOLDS_EG[ctx.depth] : LMP_THRESHOLDS_MG[ctx.depth])
        : 999;

    int moveIndex = 0;
    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        const bool isFirstMove = (moveIndex == 0);
        
        const bool wasCapture = (b.get(m.to) != chess::Board::EMPTY) || isEnPassantCapture(b, m);
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
        // CHECK EXTENSION (SELECTIVE, DETERMINISTIC)
        // =========================================================================
        // Avoid extending every checking move: that can stall depth reduction in
        // long checking sequences and hurt both speed and tactical stability.
        // Extend only forcing checks and only near the horizon.
        const bool isForcingCheck = givesCheck && (wasCapture || isPromo || moveIndex < 3);
        const bool shouldCheckExtend = isForcingCheck && (ctx.depth >= 2) && (ctx.depth <= 4);
        const int64_t childDepth = ctx.depth - 1 + (shouldCheckExtend ? 1 : 0);

        // LMR: reduce depth for late, non-critical moves
        // LOGARITHMIC LMR: reduction = floor(log(depth) * log(moveIndex) / C)
        // NOTE: nonPawnMajors/isEndgame pre-computed BEFORE loop for correctness + speed
        
        const bool inConservativeEndgameLMR = isLateEndgame && !isDelicateEndgame;
        const int lmrMinMoveIndex = inConservativeEndgameLMR ? 5 : 3;
        const bool canReduce = (ctx.depth > 2)
            && (moveIndex >= lmrMinMoveIndex)
            && !isPromo
            && (!wasCapture)
            && !givesCheck
            && !this->isKillerMove(m, killerMoves, ctx.ply)
            && !isDelicateEndgame;

        // PVS windowing:
        // - First move: full window (PV candidate)
        // - Other moves: null window, then re-search full window only on fail-high/low
        int64_t searchAlpha = bounds.alpha;
        int64_t searchBeta = bounds.beta;
        if (!isFirstMove) {
            if (usIsWhite) {
                searchBeta = bounds.alpha + 1;
            } else {
                searchAlpha = bounds.beta - 1;
            }
        }

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
            // In late endgames, keep LMR very conservative.
            if (inConservativeEndgameLMR) {
                reduction = std::min<int64_t>(reduction, 1);
            }

            const int64_t reducedDepth = std::max(static_cast<int64_t>(1), childDepth - reduction);
            score = this->searchPosition(b, reducedDepth, searchAlpha, searchBeta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);
            
            // Re-search at full depth + full window only if null-window failed.
            const bool shouldResearch = !isFirstMove && (usIsWhite
                ? (score > searchAlpha)
                : (score < searchBeta));
            
            if (shouldResearch) {
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);
            } else if (isFirstMove) {
                // Reduced first move is not expected (canReduce guards moveIndex>=3),
                // but keep behavior robust in case heuristics change.
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);
            }
        } else {
            score = this->searchPosition(b, childDepth, searchAlpha, searchBeta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);

            if (!isFirstMove) {
                const bool shouldResearch = usIsWhite
                    ? (score > searchAlpha)
                    : (score < searchBeta);
                if (shouldResearch) {
                    score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);
                }
            }
        }

        b.undoMove(m, state);

        // Track quiet moves for history malus (before checking cutoff)
        if (isQuietMove && numSearchedQuiets < MAX_QUIETS_TRACKED) {
            searchedQuiets[numSearchedQuiets++] = {m.from.index, m.to.index};
        }

        this->updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

        // Beta cutoff: check if the score causes a cutoff, then update killer/history
        // bounds.alpha >= bounds.beta means window collapsed (different condition!)
        if (isBetaCutoff(best, bounds.alpha, bounds.beta, usIsWhite)) {
            if (allowUpdates) {
                this->updateKillerAndHistoryOnBetaCutoff(b, m, ctx.depth, ctx.ply, ctx.activeColor,
                                                      history, killerMoves, ctx.previousMove);

                // HISTORY MALUS: Penalize all quiet moves searched before the cutoff move
                // These moves were tried but failed to produce a cutoff, so they deserve
                // lower history scores. This is a proven technique in top engines (+10-20 ELO).
                if (isQuietMove) { // Only if the cutoff move itself is quiet
                    const int colorIndex = (ctx.activeColor == chess::Board::WHITE) ? 0 : 1;
                    const int malus = -static_cast<int>((ctx.depth + 1) * (ctx.depth + 1));
                    // Apply malus to all quiet moves EXCEPT the last one (the cutoff move)
                    for (int i = 0; i < numSearchedQuiets - 1; ++i) {
                        history[colorIndex][searchedQuiets[i].from][searchedQuiets[i].to] += malus;
                        // Clamp to prevent going too negative
                        constexpr int MIN_HISTORY = -10000;
                        if (history[colorIndex][searchedQuiets[i].from][searchedQuiets[i].to] < MIN_HISTORY) {
                            history[colorIndex][searchedQuiets[i].from][searchedQuiets[i].to] = MIN_HISTORY;
                        }
                    }
                }
            }
            break;
        }
        ++moveIndex;
    }

    return ScoredMove{bestMove, best};
}

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply, bool useTT, bool allowTTWrite, const chess::Board::Move* previousMove, uint64_t* nodeCounter) noexcept {
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &this->nodesSearched;
    ++(*counter);

    // SAFETY CHECK: evita stack overflow e accesso fuori bounds a killerMoves/history
    if (ply >= MAX_PLY - 1) {
        return this->evaluate(b);
    }

    // QUIESCENCE SEARCH: when depth <= 0, switch to tactical-only search
    // This eliminates horizon effect by searching all captures/checks/promotions
    if (depth <= 0) {
        return this->quiescenceSearch(b, alpha, beta, ply, useTT, counter);
    }

    // PV node detection (full window vs null window), deterministic by construction.
    const bool isPVNode = (beta > alpha + 1);

    // =========================================================================
    // MATE DISTANCE PRUNING (+5-15 ELO)
    // =========================================================================
    // If we already found a mate shorter than what this node could possibly produce,
    // prune immediately. This significantly speeds up mate searches.
    // Example: if we found mate in 5, no need to search nodes at ply > 5.
    if (ply > 0) {
        // Best possible score for side to move: mate in (ply+1) moves
        // Worst possible score: getting mated in ply moves
        const int64_t matingAlpha = NEG_INF + ply;
        const int64_t matingBeta  = POS_INF - ply;
        if (alpha < matingAlpha) alpha = matingAlpha;
        if (beta > matingBeta)   beta = matingBeta;
        if (alpha >= beta) return alpha;
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

    // =========================================================================
    // INSUFFICIENT MATERIAL DRAW DETECTION (+5-10 ELO)
    // =========================================================================
    // Detect positions where neither side can deliver checkmate:
    // K vs K, K+N vs K, K+B vs K
    // Uses bitboards for fast detection (no piece counting loops).
    {
        const uint64_t wPawns  = b.pawns_bb[0],   bPawns  = b.pawns_bb[1];
        const uint64_t wRooks  = b.rooks_bb[0],   bRooks  = b.rooks_bb[1];
        const uint64_t wQueens = b.queens_bb[0],   bQueens = b.queens_bb[1];

        // Quick exit: if any pawns, rooks, or queens exist, there IS sufficient material
        if ((wPawns | bPawns | wRooks | bRooks | wQueens | bQueens) == 0ULL) {
            const uint64_t wKnights = b.knights_bb[0], bKnights = b.knights_bb[1];
            const uint64_t wBishops = b.bishops_bb[0], bBishops = b.bishops_bb[1];
            const uint64_t wMinors = wKnights | wBishops;
            const uint64_t bMinors = bKnights | bBishops;

            // K vs K
            if (wMinors == 0ULL && bMinors == 0ULL) return 0;

            // K+minor vs K (one side has exactly one minor, other has none)
            const int wMinorCount = __builtin_popcountll(wMinors);
            const int bMinorCount = __builtin_popcountll(bMinors);
            if (wMinorCount <= 1 && bMinorCount == 0) return 0; // K+N/B vs K or K vs K
            if (bMinorCount <= 1 && wMinorCount == 0) return 0; // K vs K+N/B or K vs K
        }
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
    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    const bool inCheck = b.inCheck(activeColor);
    const int nonPawnMajorsAll = __builtin_popcountll(
        b.knights_bb[0] | b.knights_bb[1] |
        b.bishops_bb[0] | b.bishops_bb[1] |
        b.rooks_bb[0]   | b.rooks_bb[1]   |
        b.queens_bb[0]  | b.queens_bb[1]);
    const bool isPawnEndgameForPruning =
        ((b.pawns_bb[0] | b.pawns_bb[1]) != 0ULL) && (nonPawnMajorsAll <= 4);

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
        
        const bool canNullMove = !isPVNode
            && !inCheck
            && ply > 0
            && depth >= 4
            && nonPawnMajors >= 3
            && evalOk;

        if (canNullMove) {
            // Adaptive reduction: R = 3 + depth/8 (less aggressive: was depth/6)
            const int64_t R = 3 + depth / 8;

            // Execute null move: flip side to move, clear en passant
            // setPrevTurn() decrements both fullMoveClock AND halfMoveClock!
            // We must save/restore them to avoid corrupting game state (50-move rule).
            const auto savedEnPassant = b.getEnPassant();
            const auto savedHalfMoveClock = b.getHalfMoveClock();
            const auto savedFullMoveClock = b.getFullMoveClock();
            b.setEnPassant(chess::Coords{}); // Clear en passant (opponent can't ep after a "pass")
            b.setNextTurn();

            const int64_t nullScore = this->searchPosition(b, depth - R, alpha, beta, ply + 1, useTT, allowTTWrite, nullptr, counter);

            // Undo null move: restore ALL state precisely
            b.setPrevTurn();
            b.setEnPassant(savedEnPassant);
            // setPrevTurn() decrements them, corrupting the 50-move rule counter
            b.restoreClocks(savedHalfMoveClock, savedFullMoveClock);

            // Check for beta cutoff
            if (usIsWhite ? (nullScore >= beta) : (nullScore <= alpha)) {
                // Verification search at reduced depth to avoid zugzwang blunders
                // Only verify if depth is high enough (otherwise overhead > benefit)
                // depth >= 10: verification is expensive, only do it at high depth
                if (depth >= 10) {
                    const int64_t verifyScore = this->searchPosition(b, depth - R, alpha, beta, ply, useTT, allowTTWrite, nullptr, counter);
                    if (usIsWhite ? (verifyScore >= beta) : (verifyScore <= alpha)) {
                        // Avoid pruning a stalemate node before terminal handling.
                        if (!b.hasAnyLegalMove(activeColor)) {
                            return stalemateScoreFromMaterialDelta(getMaterialDelta(b));
                        }
                        return usIsWhite ? beta : alpha;
                    }
                    // Verification failed: continue with full search
                } else {
                    // Avoid pruning a stalemate node before terminal handling.
                    if (!b.hasAnyLegalMove(activeColor)) {
                        return stalemateScoreFromMaterialDelta(getMaterialDelta(b));
                    }
                    return usIsWhite ? beta : alpha;
                }
            }
        }
    }

    // =========================================================================
    // REVERSE FUTILITY PRUNING (Static Null Move Pruning)
    // =========================================================================
    // If the static eval is so far above beta that even a big margin can't
    // bring it below beta, prune immediately. Much cheaper than NMP.
    // Only at very low depth where static eval is a reliable proxy.
    // CONSERVATIVE: depth <= 3 only (was 4), margin 85cp/depth (was 100)
    // At depth 3, margin = 255cp (~knight value). This avoids cutting
    // positions where the opponent has a tactical shot worth a piece.
    {
        constexpr int64_t RFP_MARGIN_PER_DEPTH = 85; // 85cp per depth level
        // Disable in pawn endgames: static-eval pruning often misses
        // pawn races, triangulation and waiting-move zugzwang motifs.
        if (!isPVNode && !inCheck && !isPawnEndgameForPruning && ply > 0 && depth <= 3) {
            const int64_t rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
            if (usIsWhite) {
                if (staticEval - rfpMargin >= beta) {
                    // Avoid pruning a stalemate node before terminal handling.
                    if (!b.hasAnyLegalMove(activeColor)) {
                        return stalemateScoreFromMaterialDelta(getMaterialDelta(b));
                    }
                    return staticEval;
                }
            } else {
                if (staticEval + rfpMargin <= alpha) {
                    // Avoid pruning a stalemate node before terminal handling.
                    if (!b.hasAnyLegalMove(activeColor)) {
                        return stalemateScoreFromMaterialDelta(getMaterialDelta(b));
                    }
                    return staticEval;
                }
            }
        }
    }

    MoveList<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.is_empty()) {
        // No legal moves: either checkmate or stalemate
        // activeColor = side that CANNOT move (stalemated side)
        
        if (inCheck) { // checkmate condition (shorter mate = better)
            return usIsWhite ? (NEG_INF + ply) : (POS_INF - ply);
        } else {
            return stalemateScoreFromMaterialDelta(getMaterialDelta(b));
        }
    }

    // Build search context (previousMove = move played by parent to reach this node)
    SearchContext ctx{depth, bounds.alpha, bounds.beta, ply, activeColor, previousMove, staticEval, inCheck, isPVNode, counter};

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
    const int64_t betaOrig = bounds.beta;

    // Search through all moves and find best move with score
    ScoredMove result = this->searchMoves(b, orderedScoredMoves, usIsWhite, ctx, bounds, useTT, allowTTWrite);
    int64_t best = result.score;

    // Save position to transposition table
    // DETERMINISM: save only if allowTTWrite=true (disabled in parallel threads)
    // Reuse hashKey computed earlier to avoid redundant computation
    if (useTT && allowTTWrite) {
        // Use the ORIGINAL search window for TT flag classification.
        // Using the mutated bounds (especially beta at minimizing nodes)
        // can incorrectly downgrade EXACT entries to LOWERBOUND.
        const auto flag = tt::determineFlag(best, alphaOrig, betaOrig);
        
        // Encode best move for TT storage
        const uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(
            result.move.from.index, result.move.to.index, result.move.promotionPiece);
        
        this->tt.store(hashKey, static_cast<uint8_t>(effectiveDepth), best, flag, encodedMove);
    }
    return best;
}

} // namespace engine
