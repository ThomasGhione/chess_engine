#include "../engine.hpp"
#include "../tt.hpp"
#include <cmath>

namespace engine {

int64_t Engine::stalemateScoreFromMaterialDelta(int64_t matDelta) noexcept {
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) return 0;
    constexpr int64_t STALEMATE_PENALTY = 5000; // penalize stalemate when the side with material advantage allows it.
    return (matDelta > 0) ? -STALEMATE_PENALTY : STALEMATE_PENALTY;
}

// Helper to handle terminal nodes and transposition table lookups
bool Engine::handleSearchPrelude(const int64_t& depth, const AlphaBeta& bounds, int64_t& score, uint64_t hashKey) noexcept {
    // Transposition table lookup (hashKey already computed by caller to avoid duplication)
    if (depth >= 2) this->tt.prefetch(hashKey);
    return this->tt.probe(hashKey, static_cast<uint8_t>(depth), bounds.alpha, bounds.beta, score);
}

// Helper to search through all moves and find best move with its score
Engine::ScoredMove Engine::searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                                       bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds, bool allowUpdates, bool allowTTWrite) noexcept {
    int64_t best = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = orderedScoredMoves[0].move;

    // =========================================================================
    // HISTORY MALUS: Track quiet moves searched before cutoff
    // =========================================================================
    // When a beta cutoff occurs, penalize all quiet moves that were searched
    // but failed to produce a cutoff to improve move ordering over time.
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
    // FUTILITY PRUNING margins (main search)
    // =========================================================================
    static constexpr int64_t FUTILITY_MARGINS_MG[] = {0, 200, 400}; // depth 0,1,2
    static constexpr int64_t FUTILITY_MARGINS_EG[] = {0, 120, 240}; // depth 0,1,2
    const bool canFutilityPrune = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;
    const int64_t futilityMargin = canFutilityPrune
        ? (isLateEndgame ? FUTILITY_MARGINS_EG[ctx.depth] : FUTILITY_MARGINS_MG[ctx.depth])
        : 0;

    // =========================================================================
    // LATE MOVE PRUNING thresholds
    // =========================================================================
    static constexpr int LMP_THRESHOLDS_MG[] = {0, 12, 20, 30}; // depth 0,1,2,3
    static constexpr int LMP_THRESHOLDS_EG[] = {0, 16, 26, 38}; // depth 0,1,2,3
    const bool canLMP = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 3 && ctx.depth >= 1;
    const int lmpThreshold = canLMP
        ? (isLateEndgame ? LMP_THRESHOLDS_EG[ctx.depth] : LMP_THRESHOLDS_MG[ctx.depth])
        : 999; // effectively disable LMP when not applicable

    const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);
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
        if (canFutilityPrune && isQuietMove && moveIndex > 0
            && shouldDeltaPrune(ctx.staticEval, futilityMargin, bounds.alpha, bounds.beta, usIsWhite)) {
            ++moveIndex;
            continue;
        }

        chess::Board::MoveState state;
        const bool isPromo = doMoveWithPromotion(b, m, state);

        // LMR: reduce depth for late, non-critical moves
        // LOGARITHMIC LMR: reduction = floor(log(depth) * log(moveIndex) / C)
        // NOTE: nonPawnMajors/isEndgame pre-computed BEFORE loop for correctness + speed
        const bool inConservativeEndgameLMR = isLateEndgame && !isDelicateEndgame;
        const int lmrMinMoveIndex = inConservativeEndgameLMR ? 5 : 3;
        const bool lmrStructuralCandidate = (ctx.depth > 2)
            && (moveIndex >= lmrMinMoveIndex)
            && !isPromo
            && (!wasCapture)
            && !isDelicateEndgame;

        const bool forcingCandidate = (wasCapture || isPromo || moveIndex < 3);
        const bool needsCheckInfo =
            (ctx.depth >= 2 && ctx.depth <= 4 && forcingCandidate) || lmrStructuralCandidate;
        const bool givesCheck = needsCheckInfo ? b.inCheck(oppColor) : false;

        // =========================================================================
        // CHECK EXTENSION (SELECTIVE, DETERMINISTIC)
        // =========================================================================
        // Avoid extending every checking move: that can stall depth reduction in
        // long checking sequences and hurt both speed and tactical stability.
        // Extend only forcing checks and only near the horizon.
        const bool isForcingCheck = givesCheck && forcingCandidate;
        const bool shouldCheckExtend = isForcingCheck && (ctx.depth >= 2) && (ctx.depth <= 4);
        const int64_t childDepth = ctx.depth - 1 + (shouldCheckExtend ? 1 : 0);
        const bool canReduce = lmrStructuralCandidate
            && !givesCheck
            && !this->isKillerMove(m, killerMoves, ctx.ply);

        // PVS windowing:
        // - First move: full window (PV candidate)
        // - Other moves: null window, then re-search full window only on fail-high/low
        const int64_t searchAlpha = isFirstMove ? bounds.alpha : (usIsWhite ? bounds.alpha : (bounds.beta - 1));
        const int64_t searchBeta  = isFirstMove ? bounds.beta  : (usIsWhite ? (bounds.alpha + 1) : bounds.beta);

        int64_t score = 0;
        if (canReduce) {
            // LOGARITHMIC LMR. Higher divisor = less reduction = more conservative
            constexpr double LMR_C = 2.75;
            int64_t reduction = static_cast<int64_t>(std::log(static_cast<double>(ctx.depth)) 
                                                   * std::log(static_cast<double>(moveIndex)) 
                                                   / LMR_C);
            // Cap reduction: never reduce more than depth-3 to ensure at least 3 plies of real search remain
            reduction = std::clamp(reduction, static_cast<int64_t>(1), ctx.depth - 3);
            
            if (inConservativeEndgameLMR) {
                reduction = std::min<int64_t>(reduction, 1);
            }

            const int64_t reducedDepth = std::max(static_cast<int64_t>(1), childDepth - reduction);
            score = this->searchPosition(b, reducedDepth, searchAlpha, searchBeta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);
            
            // Re-search at full depth + full window only if null-window failed.
            const bool shouldResearch = !isFirstMove && shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
            
            if (shouldResearch) {
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);
            }
        } else {
            score = this->searchPosition(b, childDepth, searchAlpha, searchBeta, ctx.ply + 1, allowUpdates, allowTTWrite, &m, ctx.nodeCounter);

            if (!isFirstMove) {
                const bool shouldResearch = shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
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
                // These moves were tried but failed to produce a cutoff, so they deserve lower history score
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

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply, bool useTT, bool allowTTWrite, const chess::Board::Move* previousMove, uint64_t* nodeCounter, bool allowNullMove) noexcept {
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &this->nodesSearched;
    ++(*counter);

    // SAFETY CHECK: evita stack overflow e accesso fuori bounds a killerMoves/history
    if (ply >= MAX_PLY - 1) {
        return this->evaluate(b);
    }

    if (depth <= 0) {
        return this->quiescenceSearch(b, alpha, beta, ply, useTT, counter);
    }

    // PV node detection (full window vs null window), deterministic by construction.
    const bool isPVNode = (beta > alpha + 1);

    // =========================================================================
    // MATE DISTANCE PRUNING
    // =========================================================================
    // If we already found a mate shorter than what this node could possibly produce,
    // prune immediately. This significantly speeds up mate searches.
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

    if (b.isFiftyMoveRule()) [[unlikely]] return 0; // 50-move rule detection inside search tree

    // =========================================================================
    // INSUFFICIENT MATERIAL DRAW DETECTION 
    // =========================================================================
    // Detect positions where neither side can deliver checkmate:
    // K vs K, K+N vs K, K+B vs K
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

    const uint64_t hashKey = b.getHash();
    AlphaBeta bounds{alpha, beta}; // Prepare search structures
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
    // Compute static eval ONCE for NMP, RFP, futility pruning (not in check, not at root)
    const int64_t staticEval = (ply > 0 && !inCheck) ? this->evaluate(b) : 0;

    // =========================================================================
    // NULL MOVE PRUNING
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
        const int64_t nmpEvalGate = usIsWhite ? (staticEval + 200) : (staticEval - 200);
        const bool evalOk = isBetaCutoff(nmpEvalGate, alpha, beta, usIsWhite);
        
        const bool canNullMove = allowNullMove
            && !isPVNode
            && !inCheck
            && ply > 0
            && depth >= 4
            && nonPawnMajors >= 3
            && evalOk;

        if (canNullMove) {
            const int64_t R = 3 + depth / 8; // Adaptive reduction

            chess::Board::MoveState nullState;
            b.doNullMove(nullState);

            const int64_t nullScore = this->searchPosition(
                b, depth - R, alpha, beta, ply + 1, useTT, allowTTWrite, nullptr, counter, false);

            b.undoNullMove(nullState);

            if (isBetaCutoff(nullScore, alpha, beta, usIsWhite)) {
                bool confirmedCutoff = true;
                // Verification search at reduced depth to avoid zugzwang blunders
                // Only verify if depth is high enough (otherwise overhead > benefit)
                // depth >= 10: verification is expensive, only do it at high depth
                if (depth >= 10) {
                    // Disable null-move in verification to prevent recursive NMP chains.
                    const int64_t verifyScore = this->searchPosition(
                        b, depth - R, alpha, beta, ply, useTT, allowTTWrite, nullptr, counter, false);
                    confirmedCutoff = isBetaCutoff(verifyScore, alpha, beta, usIsWhite);
                }

                if (confirmedCutoff) {
                    // Avoid pruning a stalemate node before terminal handling.
                    if (!b.hasAnyLegalMove(activeColor)) {
                        return stalemateScoreFromMaterialDelta(getMaterialDelta(b));
                    }
                    return cutoffValue(alpha, beta, usIsWhite);
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
    {
        constexpr int64_t RFP_MARGIN_PER_DEPTH = 85; // 85cp per depth level
        // Disable in pawn endgames: static-eval pruning often misses
        // pawn races, triangulation and waiting-move zugzwang motifs.
        if (!isPVNode && !inCheck && !isPawnEndgameForPruning && ply > 0 && depth <= 3) {
            const int64_t rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
            const int64_t rfpScore = usIsWhite ? (staticEval - rfpMargin) : (staticEval + rfpMargin);
            if (isBetaCutoff(rfpScore, alpha, beta, usIsWhite)) {
                // Avoid pruning a stalemate node before terminal handling.
                if (!b.hasAnyLegalMove(activeColor)) {
                    return stalemateScoreFromMaterialDelta(getMaterialDelta(b));
                }
                return staticEval;
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
    // IIR - Internal Iterative Reduction
    // =========================================================================
    // If there's no hash move from TT (the first move score < 100000, which is hash move bonus),
    // reduce depth by 1. Without a hash move, PVS is much less efficient and we'd waste
    // time searching with poor move ordering. IIR compensates by searching shallower.
    const bool hasHashMove = (!orderedScoredMoves.is_empty() && orderedScoredMoves[0].score >= 100000);
    int64_t effectiveDepth = depth;
    if (!hasHashMove && depth >= 6 && ply > 0) {
        effectiveDepth -= 1;
        ctx.depth = effectiveDepth;
    }

    const int64_t alphaOrig = bounds.alpha;
    const int64_t betaOrig = bounds.beta;

    ScoredMove result = this->searchMoves(b, orderedScoredMoves, usIsWhite, ctx, bounds, useTT, allowTTWrite);
    int64_t best = result.score;

    if (useTT && allowTTWrite) {
        const auto flag = tt::determineFlag(best, alphaOrig, betaOrig);
        
        const uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(
            result.move.from.index, result.move.to.index, result.move.promotionPiece);

        this->tt.store(hashKey, static_cast<uint8_t>(effectiveDepth), best, flag, encodedMove);
    }
    return best;
}

} // namespace engine
