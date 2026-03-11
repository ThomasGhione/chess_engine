#include "searcher.hpp"
#include "../movelist.hpp"
#include "../eval_constants.hpp"
#include <cmath>

namespace engine {

Searcher::SearchMoveResult Searcher::searchMoves(chess::Board& b, const ChessMoveList& moves,
                                                  bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds) noexcept {
    int32_t best = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = moves[0];
    bool searchedAnyMove = false;

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
    static constexpr int32_t FUTILITY_MARGINS_MG[] = {0, 260, 520}; // depth 0,1,2
    static constexpr int32_t FUTILITY_MARGINS_EG[] = {0, 170, 350}; // depth 0,1,2
    const bool canFutilityPrune = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;
    const int32_t futilityMargin = canFutilityPrune
        ? (isLateEndgame ? FUTILITY_MARGINS_EG[ctx.depth] : FUTILITY_MARGINS_MG[ctx.depth])
        : 0;

    // =========================================================================
    // LATE MOVE PRUNING thresholds
    // =========================================================================
    static constexpr int LMP_THRESHOLDS_MG[] = {0, 12, 20, 30}; // depth 0,1,2,3
    static constexpr int LMP_THRESHOLDS_EG[] = {0, 16, 26, 38}; // depth 0,1,2,3
    const bool canLMP = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;
    const int lmpThreshold = canLMP
        ? (isLateEndgame ? LMP_THRESHOLDS_EG[ctx.depth] : LMP_THRESHOLDS_MG[ctx.depth])
        : 999; // effectively disable LMP when not applicable

    const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);
    int moveIndex = 0;
    for (const auto& m : moves) {
        if (shouldAbort()) {
            state_.interrupted.store(true, std::memory_order_relaxed);
            break;
        }
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
            && Engine::shouldDeltaPrune(ctx.staticEval, futilityMargin, bounds.alpha, bounds.beta, usIsWhite)) {
            ++moveIndex;
            continue;
        }

        chess::Board::MoveState state;
        const bool isPromo = (m.promotionPiece != '\0');
        doMoveWithPromotion(b, m, state);

        // LMR: reduce depth for late, non-critical moves
        // LOGARITHMIC LMR: reduction = floor(log(depth) * log(moveIndex) / C)
        // NOTE: nonPawnMajors/isEndgame pre-computed BEFORE loop for correctness + speed
        const bool inConservativeEndgameLMR = isLateEndgame && !isDelicateEndgame;
        const int lmrMinMoveIndex = inConservativeEndgameLMR ? 14 : 12;
        const bool lmrStructuralCandidate = (ctx.depth > 6)
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
        const int32_t childDepth = ctx.depth - 1 + (shouldCheckExtend ? 1 : 0);
        const bool canReduce = lmrStructuralCandidate
            && !givesCheck
            && !isKillerMove(m, ctx.ply);

        // PVS windowing:
        // - First move: full window (PV candidate)
        // - Other moves: null window, then re-search full window only on fail-high/low
        const int32_t searchAlpha = isFirstMove ? bounds.alpha : (usIsWhite ? bounds.alpha : saturatingSub32(bounds.beta, 1));
        const int32_t searchBeta  = isFirstMove ? bounds.beta  : (usIsWhite ? saturatingAdd32(bounds.alpha, 1) : bounds.beta);

        int32_t score = 0;
        if (canReduce) {
            // LOGARITHMIC LMR. Higher divisor = less reduction = more conservative
            constexpr double LMR_C = 3.07;
            int32_t reduction = static_cast<int32_t>(std::log(static_cast<double>(ctx.depth)) 
                                                   * std::log(static_cast<double>(moveIndex)) 
                                                   / LMR_C);
            // Cap reduction: never reduce more than depth-3 to ensure at least 3 plies of real search remain
            reduction = std::clamp(reduction, static_cast<int32_t>(1), ctx.depth - 3);
            
            if (inConservativeEndgameLMR) {
                reduction = std::min<int32_t>(reduction, 1);
            }

            const int32_t reducedDepth = std::max(static_cast<int32_t>(1), childDepth - reduction);
            score = search(b, reducedDepth, searchAlpha, searchBeta, ctx.ply + 1, &m, ctx.nodeCounter);
            
            // ================================================================
            // PROPER 3-STEP LMR RE-SEARCH
            // ================================================================
            // Step 1: reduced depth + null window 
            // Step 2: if fail -> full depth + null window  
            // Step 3: if still fail -> full depth + full window (PVS re-search)
            const bool reducedFailed = Engine::shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);

            if (reducedFailed && reducedDepth < childDepth) {
                // Step 2: full depth, null window — cheap verification
                score = search(b, childDepth, searchAlpha, searchBeta, ctx.ply + 1, &m, ctx.nodeCounter);
            }

            // Step 3: full depth, full window — only if null-window still fails
            const bool shouldResearch = !isFirstMove && Engine::shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
            
            if (shouldResearch) {
                score = search(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, &m, ctx.nodeCounter);
            }
        } else {
            score = search(b, childDepth, searchAlpha, searchBeta, ctx.ply + 1, &m, ctx.nodeCounter);

            if (!isFirstMove) {
                const bool shouldResearch = Engine::shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
                if (shouldResearch) {
                    score = search(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1, &m, ctx.nodeCounter);
                }
            }
        }

        b.undoMove(m, state);
        searchedAnyMove = true;

        if (state_.interrupted.load(std::memory_order_relaxed)) {
            break;
        }

        // Track quiet moves for history malus (before checking cutoff)
        if (isQuietMove && numSearchedQuiets < MAX_QUIETS_TRACKED) {
            searchedQuiets[numSearchedQuiets++] = {m.from.index, m.to.index};
        }

        updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

        // Beta cutoff: check if the score causes a cutoff, then update killer/history
        // bounds.alpha >= bounds.beta means window collapsed (different condition!)
        if (Engine::isBetaCutoff(best, bounds.alpha, bounds.beta, usIsWhite)) {
            updateKillerAndHistoryOnBetaCutoff(b, m, ctx.depth, ctx.ply, ctx.activeColor, ctx.previousMove);

            // HISTORY MALUS: Penalize all quiet moves searched before the cutoff move
            // These moves were tried but failed to produce a cutoff, so they deserve lower history score
            if (isQuietMove) { // Only if the cutoff move itself is quiet
                const int colorIndex = (ctx.activeColor == chess::Board::WHITE) ? 0 : 1;
                const int malus = -static_cast<int>((ctx.depth + 1) * (ctx.depth + 1));
                // GRAVITY FORMULA (symmetric with bonus side in engine.cpp):
                // h += malus - h * |malus| / MAX_HISTORY
                // This naturally decays toward bounds instead of hard-clamping,
                // which caused asymmetric ordering: bad sacrifices weren't
                // penalised fast enough relative to how quickly good moves
                // were promoted.
                constexpr int32_t MAX_HISTORY = 16384;
                for (int i = 0; i < numSearchedQuiets - 1; ++i) {
                    int16_t& h = state_.history[colorIndex][searchedQuiets[i].from][searchedQuiets[i].to];
                    int32_t hScore = static_cast<int32_t>(h);
                    hScore += malus - hScore * std::abs(malus) / MAX_HISTORY;
                    h = clampHeuristic16(hScore);
                }
            }
            break;
        }
        ++moveIndex;
    }

    if (!searchedAnyMove && state_.interrupted.load(std::memory_order_relaxed)) {
        return SearchMoveResult{bestMove, ctx.staticEval};
    }

    return SearchMoveResult{bestMove, best};
}


bool Searcher::sortLegalMoves(ChessMoveList& moves, int ply, chess::Board& b,
                              bool usIsWhite, uint64_t hashKey, const chess::Board::Move* previousMove) noexcept {
    if (moves.is_empty()) [[unlikely]] return false;

    int32_t moveScores[MAX_MOVES] {};

    // Precompute expensive variables outside the loop
    const bool inCheck = b.inCheck(b.getActiveColor());
    const int fullMoveClock = b.getFullMoveClock();
    const int nonPawnMajors = __builtin_popcountll(
        b.knights_bb[0] | b.knights_bb[1] |
        b.bishops_bb[0] | b.bishops_bb[1] |
        b.rooks_bb[0]   | b.rooks_bb[1]   |
        b.queens_bb[0]  | b.queens_bb[1]);
    const bool isEndgameOrdering = (nonPawnMajors <= 5);
    const int usSide = chess::Board::colorBoolToIndex(usIsWhite);
    const int oppSide = usSide ^ 1;
    const uint64_t occ = b.getPiecesBitMap();
    const uint64_t oppKingBB = b.kings_bb[oppSide];
    const uint8_t oppKingSq = oppKingBB ? static_cast<uint8_t>(__builtin_ctzll(oppKingBB)) : 64;
    const uint8_t promotionRank = chess::Board::promotionRank(usIsWhite);
    const chess::Coords enPassant = b.getEnPassant();
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);

    // HASH MOVE: Retrieve from TT for highest priority
    uint16_t encodedHashMove = 0;
    uint8_t hashFrom = 64, hashTo = 64;
    char hashPromo = '\0';
    bool hashMoveIsLegal = false;
    
    // Probe TT with move-only API (no alpha/beta/score overhead).
    if (tt_.probeMove(hashKey, encodedHashMove)) {
        tt::TranspositionTable::Entry::decodeMove(encodedHashMove, hashFrom, hashTo, hashPromo);

        // Validate hash move is in legal moves list (guards against TT collisions)
        hashMoveIsLegal = Engine::containsMoveWithPromotion(moves, hashFrom, hashTo, hashPromo);
    }

    // Score all moves once, then reorder moves in-place.
    for (int moveIndex = 0; moveIndex < moves.size; ++moveIndex) {
        const auto& m = moves[moveIndex];
        const uint8_t fromPiece = b.get(m.from);
        const uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;

        const uint8_t toPiece = b.get(m.to);
        const uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isEpCapture = hasEnPassant
            && fromPieceType == chess::Board::PAWN
            && toPieceType == chess::Board::EMPTY
            && (m.to == enPassant)
            && (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index));
        const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
        const uint8_t victimType = isEpCapture ? static_cast<uint8_t>(chess::Board::PAWN) : toPieceType;
        const bool isPromotionCandidate = (fromPieceType == chess::Board::PAWN) && (m.to.rank() == promotionRank);
        const int32_t see = isCapture ? staticExchangeEvaluation(b, m) : 0;
        
        int32_t score = Engine::scoreMoveOrderingPriorityInline(
            b, m, fromPieceType, isCapture, victimType, see, isPromotionCandidate, moveIndex,
            hashMoveIsLegal, hashFrom, hashTo, hashPromo, ply, previousMove, usSide, oppKingSq, occ,
            usIsWhite, isEndgameOrdering, fullMoveClock, state_.history, state_.killerMoves, state_.counterMoves,
            state_.captureHistory, PIECE_VALUES, ORDERING_PENALTY_SAME_PAWN_OPENING);

        // NOTE: Stalemate check removed from move ordering (too expensive: doMove/undoMove per move!)
        // Stalemate is now handled ONLY in searchPosition() terminal node evaluation
        // This is much faster and still prevents stalemate in winning positions

        // King move penalties (lower king-move priority in the opening if not castling)
        if (fromPieceType == chess::Board::KING) {
            const int fileDelta = std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index));
            const bool isCastling = (fileDelta == 2);

            if (fullMoveClock < 10 && !inCheck && !isCastling) {
                score -= 220; // opening-only ordering penalty
            } else if (isCastling) {
                score += 550; // keep castling high priority without overpowering tactical quiets
            }
        }

        moveScores[moveIndex] = score;
    }

    // Insertion-sort scores and moves together (descending score).
    // MAX_MOVES is small, and the list is often partially ordered.
    for (int i = 1; i < moves.size; ++i) {
        const chess::Board::Move keyMove = moves[static_cast<size_t>(i)];
        const int32_t keyScore = moveScores[i];
        int j = i - 1;
        while (j >= 0 && moveScores[j] < keyScore) {
            moveScores[j + 1] = moveScores[j];
            moves[static_cast<size_t>(j + 1)] = moves[static_cast<size_t>(j)];
            --j;
        }
        moveScores[j + 1] = keyScore;
        moves[static_cast<size_t>(j + 1)] = keyMove;
    }

    return hashMoveIsLegal
        && !moves.is_empty()
        && Engine::sameFromTo(moves[0], hashFrom, hashTo)
        && moves[0].promotionPiece == hashPromo;
}

} // namespace engine
