#include "searcher.hpp"

#include <iostream>
#include <numeric>

#include "../engine.hpp"
#include "../eval/evaluator.hpp"
#include "../sort/move_generator.hpp"

namespace engine {

namespace {

// PAWN_VALUE is a runtime-mutable eval global, so this stays a (non-constexpr)
// local; every other search parameter lives in search_constants.hpp.
const int32_t REPETITION_DRAW_ADVANTAGE_THRESHOLD = PAWN_VALUE / 2;

// Precomputed LMR reductions: LMR_TABLE[depth][moveIndex], capped at depth-3.
// Avoids two std::log() calls per LMR candidate in the hot search loop.
struct LMRTable {
    int8_t data[LMR_MAX_DEPTH][LMR_MAX_MOVES]{};
    constexpr LMRTable() noexcept {
        for (int d = 0; d < LMR_MAX_DEPTH; ++d) {
            for (int m = 0; m < LMR_MAX_MOVES; ++m) {
                if (d == 0 || m == 0) { data[d][m] = 1; continue; }
                const double raw = __builtin_log(static_cast<double>(d))
                                 * __builtin_log(static_cast<double>(m))
                                 / LMR_C;
                const int r = static_cast<int>(raw);
                data[d][m] = static_cast<int8_t>(std::max(1, std::min(r, d - 3)));
            }
        }
    }
};
static constexpr LMRTable LMR_REDUCTION_TABLE;

inline size_t corrHistIndex(uint64_t a, uint64_t c) noexcept {
    const uint64_t k = a * 0x9E3779B97F4A7C15ULL + c * 0xD6E8FEB86659FD93ULL;
    return static_cast<size_t>(k >> 48) & (PAWN_CORR_HISTORY_SIZE - 1);
}

inline size_t pawnCorrIndex(const chess::Board& b) noexcept {
    return corrHistIndex(b.pawns_bb[0], b.pawns_bb[1]);
}
inline size_t minorCorrIndex(const chess::Board& b) noexcept {
    return corrHistIndex(b.knights_bb[0] | b.bishops_bb[0],
                         b.knights_bb[1] | b.bishops_bb[1]);
}
inline size_t majorCorrIndex(const chess::Board& b) noexcept {
    return corrHistIndex(b.rooks_bb[0] | b.queens_bb[0],
                         b.rooks_bb[1] | b.queens_bb[1]);
}

inline int32_t evalCorrection(const SearchRuntime& runtime, const chess::Board& b) noexcept {
    const int side = chess::Board::colorToIndex(b.getActiveColor());
    const int32_t c = runtime.pawnCorrHist[side][pawnCorrIndex(b)]  / CORR_HIST_DIVISOR
                    + runtime.minorCorrHist[side][minorCorrIndex(b)] / CORR_HIST_DIVISOR
                    + runtime.majorCorrHist[side][majorCorrIndex(b)] / CORR_HIST_DIVISOR;
    return std::clamp(c, -CORR_TOTAL_CAP, CORR_TOTAL_CAP);
}

} // namespace

// --- Negamax helpers ---
constexpr bool Searcher::isBetter(int32_t newScore, int32_t currentBest) noexcept {
    return newScore > currentBest;
}

constexpr bool Searcher::isBetaCutoff(int32_t score, int32_t beta) noexcept {
    return score >= beta;
}

void Searcher::updateBound(int32_t score, int32_t& alpha) noexcept {
    if (score > alpha) alpha = score;
}

constexpr bool Searcher::shouldDeltaPrune(int32_t standPat, int32_t margin, int32_t alpha) noexcept {
    return standPat + margin <= alpha;
}

constexpr bool Searcher::shouldResearchPVS(int32_t score, int32_t alphaBound) noexcept {
    return score > alphaBound;
}


constexpr int32_t Searcher::saturatingAdd32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t sum = static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs);
    return static_cast<int32_t>(std::clamp<int64_t>(sum, NEG_INF, POS_INF));
}

constexpr int32_t Searcher::saturatingSub32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t diff = static_cast<int64_t>(lhs) - static_cast<int64_t>(rhs);
    return static_cast<int32_t>(std::clamp<int64_t>(diff, NEG_INF, POS_INF));
}

constexpr int32_t Searcher::scoreToTT(int32_t score, int ply) noexcept {
    if (score >= MATE_BOUND)  return saturatingAdd32(score, ply);
    if (score <= -MATE_BOUND) return saturatingSub32(score, ply);
    return score;
}

constexpr int32_t Searcher::scoreFromTT(int32_t score, int ply) noexcept {
    if (score >= MATE_BOUND)  return saturatingSub32(score, ply);
    if (score <= -MATE_BOUND) return saturatingAdd32(score, ply);
    return score;
}

void Searcher::applyHistoryGravity(int16_t& cell, int32_t delta, int32_t maxValue) noexcept {
    const int32_t magnitude = (delta < 0) ? -delta : delta;
    int32_t value = cell;
    value += delta - value * magnitude / maxValue;
    constexpr int32_t MAX_I16 = 32767; // fits in int16_t after clamping
    constexpr int32_t MIN_I16 = -32768; // fits in int16_t after clamping
    cell = static_cast<int16_t>(std::clamp(value, MIN_I16, MAX_I16));
}

void Searcher::writeTT(SearchRuntime& runtime, uint64_t hashKey, int32_t depth,
                       int32_t best, int32_t alphaOrig, int32_t betaOrig, int ply) noexcept {
    const auto flag = determineFlag(best, alphaOrig, betaOrig);
    runtime.transpositionTable->store(
        hashKey, static_cast<uint8_t>(depth),
        static_cast<int32_t>(std::clamp<int64_t>(scoreToTT(best, ply), NEG_INF, POS_INF)), static_cast<uint8_t>(flag));
}

void Searcher::writeTT(SearchRuntime& runtime, uint64_t hashKey, int32_t depth,
                       int32_t best, int32_t alphaOrig, int32_t betaOrig, int ply,
                       const chess::Board::Move& bestMove) noexcept {
    const auto flag = determineFlag(best, alphaOrig, betaOrig);
    const uint16_t encodedMove = TranspositionTable::Entry::encodeMove(
        bestMove.from.index, bestMove.to.index, bestMove.promotionPiece);
    runtime.transpositionTable->store(
        hashKey, static_cast<uint8_t>(depth),
        static_cast<int32_t>(std::clamp<int64_t>(scoreToTT(best, ply), NEG_INF, POS_INF)), static_cast<uint8_t>(flag), encodedMove);
}

bool Searcher::checkEarlyTerminalConditions(
    const chess::Board& b,
    SearchRuntime& runtime,
    int ply,
    int32_t& outScore) noexcept {
    
    if (runtime.shouldAbort()) {
        runtime.markInterrupted();
        outScore = Evaluator::evaluate(b);
        return true;
    }

    if (ply >= MAX_PLY - 1) {
        outScore = Evaluator::evaluate(b);
        return true;
    }

    // Terminal king-capture states are possible in this codebase's move model.
    // Negamax / side-to-move relative: losing our own king is a mated score
    // (NEG_INF + ply); capturing the opponent's king is a winning score.
    if (b.kings_bb[0] == 0ULL || b.kings_bb[1] == 0ULL) {
        const bool whiteToMove = (b.getActiveColor() == chess::Board::WHITE);
        const bool ourKingGone = (b.kings_bb[0] == 0ULL) ? whiteToMove : !whiteToMove;
        outScore = ourKingGone ? (NEG_INF + ply) : (POS_INF - ply);
        return true;
    }

    return false;
}

bool Searcher::checkDrawTerminalConditions(
    const chess::Board& b,
    int32_t& outScore,
    bool atRoot) noexcept {
    const int repCount = b.countRepetitions();

    // Third repetition: forced draw — apply full contempt penalty.
    if (repCount >= 3) {
        outScore = repetitionDrawScore(b);
        return true;
    }

    // Second repetition: not yet a forced draw, but scores as 0.
    // This prevents the engine from "chasing" draws when winning (alpha > 0
    // won't be improved by a 0 score, so the engine is forced to find real moves).
    //
    // This is an INTERIOR-NODE heuristic only. At the root the current position
    // is not a draw under FIDE rules until it actually occurs a third time, so
    // returning here would abandon the search and play moves[0] — a random legal
    // move that, as observed, can hang the queen. At the root we must search.
    if (!atRoot && repCount >= 2) {
        outScore = 0;
        return true;
    }

    if (b.isFiftyMoveRule()) [[unlikely]] {
        outScore = 0;
        return true;
    }

    if (b.hasInsufficientMaterialDraw()) [[unlikely]] {
        outScore = 0;
        return true;
    }

    return false;
}


int32_t Searcher::stalemateScoreFromMaterialDelta(int32_t matDelta) noexcept {
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) return 0;
    const int32_t advantage = std::abs(matDelta);
    const int32_t scaledPenalty =
        STALEMATE_DRAW_PENALTY_MINOR + (advantage - STALEMATE_MATERIAL_THRESHOLD) / 2;
    const int32_t stalematePenalty = std::clamp<int32_t>(
        scaledPenalty, STALEMATE_DRAW_PENALTY_MINOR, STALEMATE_DRAW_PENALTY_MAJOR);
    return (matDelta > 0) ? -stalematePenalty : stalematePenalty;
}

bool Searcher::tryStalemateScore(const chess::Board& b, const SearchNodeState& node, int32_t& outScore) noexcept {
    if (b.hasAnyLegalMove(node.activeColor)) return false;
    const int32_t md = b.getIncrementalMaterialDelta();
    outScore = stalemateScoreFromMaterialDelta((node.activeColor == chess::Board::WHITE) ? md : -md);
    return true;
}

int32_t Searcher::drawAdvantageScore(const chess::Board& b) noexcept {
    // Negamax: make the white-centric material delta side-to-move relative;
    // Evaluator::evaluate() is already STM-relative.
    const int32_t mdWhite = b.getIncrementalMaterialDelta();
    const int32_t materialDelta =
        (b.getActiveColor() == chess::Board::WHITE) ? mdWhite : -mdWhite;
    const int32_t staticEval = Evaluator::evaluate(b);
    const int64_t blendedScore =
        static_cast<int64_t>(materialDelta) * DRAW_SCORE_MATERIAL_WEIGHT_PERCENT
        + static_cast<int64_t>(staticEval) * DRAW_SCORE_EVAL_WEIGHT_PERCENT;

    return static_cast<int32_t>(std::clamp<int64_t>(blendedScore / DRAW_SCORE_WEIGHT_DENOMINATOR, NEG_INF, POS_INF));
}

int32_t Searcher::repetitionDrawScore(const chess::Board& b) noexcept {
    const int32_t drawDelta = drawAdvantageScore(b);
    if (std::abs(drawDelta) <= REPETITION_DRAW_ADVANTAGE_THRESHOLD) {
        return 0;
    }

    // Modest fixed contempt: enough that the engine prefers to keep playing
    // when winning, but smaller than any meaningful material amount so it
    // won't trade a piece just to avoid the repetition.
    return (drawDelta > 0) ? -REPETITION_CONTEMPT : REPETITION_CONTEMPT;
}

void Searcher::updateMinMax(
    int32_t score,
    int32_t& alpha,
    int32_t& bestScore,
    chess::Board::Move& bestMove,
    const chess::Board::Move& m) noexcept {
    const bool better = isBetter(score, bestScore);
    bestScore = better ? score : bestScore;
    bestMove = better ? m : bestMove;

    updateBound(score, alpha);
}

chess::Board::Move Searcher::searchBestMove(
    chess::Board& board,
    SearchRuntime& runtime,
    uint64_t requestedDepth) noexcept {
    const uint64_t targetDepth = (requestedDepth == 0)
        ? DEFAULT_DEPTH
        : requestedDepth;

    runtime.clearInterrupted();

    if (runtime.transpositionTable != nullptr) {
        runtime.transpositionTable->incrementGeneration();
    }
    runtime.nodesSearched = 0;
    runtime.softResetHistory();

    IterativeSearchResult result = runIterativeDeepening(board, runtime, 1, targetDepth);
    runtime.depth = targetDepth;

    // Completed/terminal result, else a deterministic fallback move.
    if (result.terminalRoot || result.completedAnyDepth) {
        runtime.eval = result.bestScore;
        return result.bestMove;
    }

    MoveList fallbackMoves = engine::MoveGenerator::generateLegalMoves(board);
    runtime.eval = Evaluator::evaluate(board);
    return fallbackMoves.is_empty() ? chess::Board::Move{} : fallbackMoves[0];
}

int32_t Searcher::searchRootMoveScore(
    chess::Board& b,
    const chess::Board::Move& m,
    SearchRuntime& runtime,
    int32_t alpha,
    int32_t beta,
    bool allowTTWrite,
    bool allowHeuristicUpdates,
    uint64_t* nodeCounter) noexcept {
    chess::Board::MoveState state;
    b.doMove(m, state);
    // Negamax: child is the opponent to move -> negate and swap/negate bounds.
    // runtime.depth is uint64_t; guard the subtraction so an unexpected depth
    // of 0 cannot underflow to UINT64_MAX and reinterpret as a negative int32_t.
    // In normal IDS flow runtime.depth is always >= 1, but tests/direct callers
    // could pass through with 0.
    const int32_t childDepth = (runtime.depth > 0)
        ? static_cast<int32_t>(runtime.depth - 1)
        : 0;
    const int32_t score = -searchPosition(
        b, runtime, childDepth, -beta, -alpha, 1,
        true, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter);
    b.undoMove(m, state);
    return score;
}

bool Searcher::handleSearchPrelude(
    const SearchRuntime& runtime,
    int32_t depth,
    const AlphaBeta& bounds,
    int32_t& score,
    uint64_t hashKey,
    int ply) noexcept {
    // Precondition (guaranteed by the only caller's canUseTT): transpositionTable != nullptr.
    if (depth >= 2) runtime.transpositionTable->prefetch(hashKey);

    int32_t ttScore = 0;
    if (runtime.transpositionTable->probe(hashKey, static_cast<uint8_t>(depth), bounds.alpha, bounds.beta, ttScore)) {
        score = scoreFromTT(ttScore, ply); // re-base mate scores to this node's ply
        return true;
    }
    return false;
}

bool Searcher::tryNullMovePruning(
    chess::Board& b,
    const SearchNodeState& node,
    SearchRuntime& runtime,
    int32_t depth,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool useTT,
    bool allowTTWrite,
    bool allowHeuristicUpdates,
    uint64_t* nodeCounter,
    int32_t& outScore) noexcept {
    // Eval-scaled reduction: the further static eval sits above beta, the more
    // certain the null move fails high, so reduce deeper. Clamped to >= 0 (the
    // NMP gate allows eval up to ~100cp below beta) and capped.
    const int32_t evalReduction = std::clamp((node.staticEval - beta) / NMP_EVAL_DIV, 0, NMP_EVAL_MAX);
    const int32_t reduction = 3 + depth / 3 + evalReduction;

    chess::Board::MoveState nullState;
    b.doNullMove(nullState);

    // Negamax: after the null move it is the opponent to move.
    const int32_t nullScore = -searchPosition(
        b, runtime, depth - reduction, -beta, -alpha, ply + 1,
        useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter, false);

    b.undoNullMove(nullState);

    if (!isBetaCutoff(nullScore, beta)) {
        return false;
    }

    bool confirmedCutoff = true;
    if (depth >= NULL_MOVE_VERIFICATION_DEPTH) {
        // Verification re-search of THIS node (same side to move): no negation.
        const int32_t verifyScore = searchPosition(
            b, runtime, depth - reduction, alpha, beta, ply,
            useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter, false);
        confirmedCutoff = isBetaCutoff(verifyScore, beta);
    }

    if (!confirmedCutoff) {
        return false;
    }

    if (tryStalemateScore(b, node, outScore)) {
        return true;
    }

    // Fail-soft: return the null-search score (a heuristic lower bound), but
    // never propagate an unproven mate found behind a null move.
    outScore = (nullScore >= MATE_BOUND) ? beta : nullScore;
    return true;
}

bool Searcher::tryReverseFutilityPruning(
    const chess::Board& b,
    const SearchNodeState& node,
    int32_t depth,
    int32_t beta,
    int32_t& outScore) noexcept {
    // Precondition (guaranteed by the only caller's canReverseFutilityPrune):
    // !isPVNode && !inCheck && !isPawnEndgameForPruning && ply > 0 && depth <= 3.

    // Negamax: staticEval is side-to-move relative; fail high if it beats
    // beta even after subtracting the margin.
    const int32_t rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
    const int32_t rfpScore = node.staticEval - rfpMargin;
    if (!isBetaCutoff(rfpScore, beta)) {
        return false;
    }

    if (tryStalemateScore(b, node, outScore)) {
        return true;
    }

    outScore = node.staticEval;
    return true;
}

void Searcher::updateKillerAndHistoryOnBetaCutoff(
    const chess::Board::Move& m,
    bool isCapture,
    int victimType,
    int32_t depth,
    int ply,
    uint8_t us,
    SearchRuntime& runtime,
    const chess::Board::Move* previousMove,
    int16_t* contHistEntry,
    int fromPieceType) noexcept {
    if (ply < 0 || ply >= MAX_PLY) return;

    const int fromIndex = m.from.index;
    const int toIndex = m.to.index;
    const int usSide = chess::Board::colorToIndex(us);
    const int32_t depthPlusOne = depth + 1;
    const int32_t bonus = depthPlusOne * depthPlusOne;

    // CAPTURE HISTORY: bonus for captures that cause cutoffs.
    if (isCapture) {
        auto& chPrimary = runtime.captureHistory[usSide][toIndex][victimType][0];
        auto& chSecondary = runtime.captureHistory[usSide][toIndex][victimType][1];
        applyHistoryGravity(chPrimary, bonus, MAX_CAPTURE_HISTORY);
        applyHistoryGravity(chSecondary, bonus >> 1, MAX_CAPTURE_HISTORY);
        if (chSecondary > chPrimary) {
            std::swap(chPrimary, chSecondary);
        }

        return;
    }

    // COUNTER-MOVE: best response to previous quiet move.
    if (previousMove != nullptr && previousMove->from.index < 64) {
        runtime.counterMoves[previousMove->from.index][previousMove->to.index] =
            TranspositionTable::Entry::encodeMove(fromIndex, toIndex, m.promotionPiece);
    }

    // KILLER MOVES: update while avoiding duplicates.
    auto& km1 = runtime.killerMoves[0][ply];
    const bool isAlreadyKm1 = (fromIndex == km1.from.index) && (toIndex == km1.to.index);
    if (!isAlreadyKm1) {
        auto& km2 = runtime.killerMoves[1][ply];
        km2 = km1;
        km1 = m;
    }

    // HISTORY HEURISTIC: gravity update.
    applyHistoryGravity(runtime.history[usSide][fromIndex][toIndex], bonus, MAX_HISTORY);

    // CONTINUATION HISTORY: bonus for this move given the previous move.
    if (contHistEntry != nullptr) {
        applyHistoryGravity(contHistEntry[contHistIndex(fromPieceType, toIndex)], bonus, MAX_HISTORY);
    }
}

Searcher::SearchMoveResult Searcher::searchMoves(
    chess::Board& b,
    MovePicker& movePicker,
    const SearchContext& ctx,
    AlphaBeta& bounds,
    SearchRuntime& runtime,
    bool useTT,
    bool allowHeuristicUpdates,
    bool allowTTWrite) noexcept {
    const bool usIsWhite = (ctx.activeColor == chess::Board::WHITE);
    int32_t best = NEG_INF;
    chess::Board::Move bestMove{};
    bool searchedAnyMove = false;

    struct QuietEntry { uint8_t from; uint8_t to; uint8_t pieceType; };
    constexpr int MAX_QUIETS_TRACKED = 64;
    QuietEntry searchedQuiets[MAX_QUIETS_TRACKED];
    int numSearchedQuiets = 0;

    struct CaptureEntry { uint8_t to; uint8_t victimType; };
    constexpr int MAX_CAPTURES_TRACKED = 32;
    CaptureEntry searchedCaptures[MAX_CAPTURES_TRACKED];
    int numSearchedCaptures = 0;

    const int nonPawnMajorsForLMR = b.getIncrementalNonPawnMajorCount();
    const bool isLateEndgame = (nonPawnMajorsForLMR <= 5);

    const bool canFutilityPrune =
        !ctx.isPVNode && !ctx.inCheck && ctx.ply > 0 && ctx.depth >= 1 && ctx.depth <= 6 && !ctx.improving;
    const int32_t futilityMargin = canFutilityPrune ? FUTILITY_MARGINS[isLateEndgame][ctx.depth] : 0;

    const bool canLMP =
        !ctx.isPVNode && !ctx.inCheck && ctx.ply > 0 && ctx.depth >= 1 && ctx.depth <= 4;
    const int lmpThreshold = canLMP ? LMP_THRESHOLDS[ctx.improving][isLateEndgame][ctx.depth] : 999;

    const int usSide = chess::Board::colorToIndex(ctx.activeColor);
    const int oppSide = usSide ^ 1;
    const uint64_t oppKingBBForFutility = b.kings_bb[oppSide];
    const int oppKingSq = oppKingBBForFutility ? std::countr_zero(oppKingBBForFutility) : 64;
    const chess::Coords enPassant = b.getEnPassant();
    const int promotionRank = chess::Board::promotionRank(usIsWhite);

    const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);

    while (movePicker.hasNext()) {
        const int moveIndex = movePicker.currentIndex;
        const chess::Board::Move m = movePicker.nextMove();
        
        if (runtime.shouldAbort()) {
            runtime.markInterrupted();
            break;
        }

        if (ctx.excludedMove.from.isValid()
            && m.from.index == ctx.excludedMove.from.index
            && m.to.index   == ctx.excludedMove.to.index) {
            continue;
        }

        const bool isFirstMove = (moveIndex == 0);

        const int fromIndex = m.from.index;
        const int toIndex = m.to.index;
        const int fromPieceType = b.get(fromIndex) & chess::Board::MASK_PIECE_TYPE;
        const int toPieceType = b.get(toIndex) & chess::Board::MASK_PIECE_TYPE;
        const bool isPawnMove = (fromPieceType == chess::Board::PAWN);
        const auto cap = Sorter::classifyCapture(m, fromPieceType, toPieceType, enPassant);
        const bool wasCapture = cap.isCapture;
        const int victimType = cap.victimType;
        const bool isPromotionCandidate = isPawnMove && (m.to.rank() == promotionRank);
        const bool isQuietMove = !wasCapture && !isPromotionCandidate;

        if (canLMP && isQuietMove && moveIndex >= lmpThreshold) {
            continue;
        }

        // Pre-move check detection for futility, computed lazily only on the
        // quiet moves that can actually be futility/LMP-pruned.
        bool preMoveGivesCheck = false;
        if ((canFutilityPrune || (canLMP && isQuietMove))
            && isQuietMove && fromPieceType != chess::Board::KING
            && oppKingSq < 64) {
            preMoveGivesCheck = Sorter::givesCheckFast(b, m, fromPieceType, oppKingSq,
                                                       b.getPiecesBitMap());
        }

        if (canFutilityPrune && isQuietMove && !preMoveGivesCheck && moveIndex > 0
            && shouldDeltaPrune(ctx.staticEval, futilityMargin, bounds.alpha)) {
            continue;
        }

        // History-based quiet pruning: skip late quiet moves with very negative
        // history at low depth — they reliably fail to improve alpha.
        if (isQuietMove && !ctx.isPVNode && !ctx.inCheck && ctx.ply > 0
            && ctx.depth >= 1 && ctx.depth <= 3 && moveIndex > 0) {
            const int32_t histScore = runtime.history[usSide][m.from.index][m.to.index];
            if (histScore < HISTORY_PRUNE_THRESHOLD[ctx.depth]) {
                continue;
            }
        }

        // SEE pruning: skip clearly losing captures at shallow non-PV nodes.
        // Move 0 (a winning or hash capture) is always searched; the margin
        // scales with depth so deeper nodes tolerate slightly worse trades.
        if (wasCapture && !isPromotionCandidate && moveIndex > 0
            && !ctx.isPVNode && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 3
            && Sorter::staticExchangeEvaluationPublic(b, m) < -SEE_CAPTURE_MARGIN * ctx.depth) {
            continue;
        }

        chess::Board::MoveState state;
        b.doMove(m, state);

        // Late captures are reduced too: move ordering ranks good captures early,
        // so a capture reaching this index is almost always a bad/losing one.
        const bool lmrStructuralCandidate = (ctx.depth >= 4)
            && (moveIndex >= 4)
            && !isPromotionCandidate;

        const bool forcingCandidate = (wasCapture || isPromotionCandidate || moveIndex < 3);
        const bool needsCheckInfo =
            (ctx.depth >= 2 && ctx.depth <= 4 && forcingCandidate) || lmrStructuralCandidate;
        const bool givesCheck = needsCheckInfo ? b.inCheck(oppColor) : false;

        const bool isForcingCheck = givesCheck && forcingCandidate;
        const bool shouldCheckExtend = isForcingCheck && (ctx.depth >= 2) && (ctx.depth <= 4);
        const int32_t childDepth = ctx.depth - 1 + (shouldCheckExtend ? 1 : 0)
                                 + (isFirstMove ? ctx.singularExtension : 0);
        const auto& km0 = runtime.killerMoves[0][ctx.ply];
        const auto& km1 = runtime.killerMoves[1][ctx.ply];
        const bool isKiller = (m.from.index == km0.from.index && m.to.index == km0.to.index)
                           || (m.from.index == km1.from.index && m.to.index == km1.to.index);
        const bool canReduce = lmrStructuralCandidate
            && !givesCheck
            && !isKiller;

        // Negamax PVS scout window: full [alpha,beta] for the first move,
        // null window [alpha, alpha+1] for the rest.
        const int32_t scoutAlpha = bounds.alpha;
        const int32_t scoutBeta  = isFirstMove ? bounds.beta
                                               : saturatingAdd32(bounds.alpha, 1);

        int32_t score = 0;
        if (canReduce) {
            const int di = ctx.depth < LMR_MAX_DEPTH ? ctx.depth : LMR_MAX_DEPTH - 1;
            const int mi = moveIndex < LMR_MAX_MOVES ? moveIndex : LMR_MAX_MOVES - 1;
            int32_t reduction = LMR_REDUCTION_TABLE.data[di][mi];

            if (ctx.isPVNode) {
                reduction = std::max(1, reduction - 1);
            }
            if (ctx.iirActive) {
                reduction = std::min(reduction + 1, childDepth - 1);
            }
            // Reduce more when the position is not improving: a late quiet move
            // is less likely to help when our static eval is falling.
            if (!ctx.improving) {
                reduction += 1;
            }
            // History adjustment (quiet moves only — quiet history is meaningless
            // for captures, which the ordering already ranks by SEE/capture history).
            if (!wasCapture) {
                const int32_t histScore = runtime.history[usSide][m.from.index][m.to.index];
                reduction -= histScore / 8192;
            }
            reduction = std::clamp(reduction, 1, childDepth - 1);

            const int32_t reducedDepth = std::max(1, childDepth - reduction);
            score = -searchPosition(b, runtime, reducedDepth, -scoutBeta, -scoutAlpha, ctx.ply + 1,
                                    useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
            
            if (shouldResearchPVS(score, scoutAlpha)) {
                score = -searchPosition(b, runtime, childDepth, -scoutBeta, -scoutAlpha, ctx.ply + 1,
                                        useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
            }
        } else { // can't reduce, regular PVS search
            score = -searchPosition(b, runtime, childDepth, -scoutBeta, -scoutAlpha, ctx.ply + 1,
                                    useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
        }

        // Wide-window PV re-search, shared by both paths above. It only carries
        // information in PV nodes: in a non-PV node beta == alpha+1 == scoutBeta,
        // so this window equals the null-window scout/research and would just
        // re-probe the TT for an identical result. Gate on isPVNode to avoid the
        // duplicate node. (In the reduce path !isFirstMove always holds.)
        if (ctx.isPVNode && !isFirstMove && shouldResearchPVS(score, scoutAlpha)) {
            score = -searchPosition(b, runtime, childDepth, -bounds.beta, -bounds.alpha, ctx.ply + 1,
                                    useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
        }

        b.undoMove(m, state);
        searchedAnyMove = true;

        if (runtime.isInterrupted()) {
            break;
        }

        if (isQuietMove && numSearchedQuiets < MAX_QUIETS_TRACKED) {
            searchedQuiets[numSearchedQuiets++] =
                {static_cast<uint8_t>(m.from.index), static_cast<uint8_t>(m.to.index),
                 static_cast<uint8_t>(fromPieceType)};
        }
        if (wasCapture && numSearchedCaptures < MAX_CAPTURES_TRACKED) {
            searchedCaptures[numSearchedCaptures++] = {m.to.index, static_cast<uint8_t>(victimType)};
        }

        updateMinMax(score, bounds.alpha, best, bestMove, m);

        if (isBetaCutoff(best, bounds.beta)) {
            if (allowHeuristicUpdates) {
                updateKillerAndHistoryOnBetaCutoff(
                    m, wasCapture, victimType, ctx.depth, ctx.ply, ctx.activeColor, runtime, ctx.previousMove, ctx.contHistEntry, fromPieceType);

                const int32_t depthPlusOne = ctx.depth + 1;
                const int32_t malus = -(depthPlusOne * depthPlusOne);

                // Malus to quiet moves searched before the cutoff. When the
                // cutoff move is itself quiet it is the last tracked entry and
                // is excluded (it gets the bonus instead); when the cutoff is a
                // capture, every tracked quiet failed and is penalised.
                const int quietMalusEnd = numSearchedQuiets - (isQuietMove ? 1 : 0);
                for (int i = 0; i < quietMalusEnd; ++i) {
                    applyHistoryGravity(runtime.history[usSide][searchedQuiets[i].from][searchedQuiets[i].to],
                                        malus, MAX_HISTORY);
                    if (ctx.contHistEntry != nullptr) {
                        applyHistoryGravity(
                            ctx.contHistEntry[contHistIndex(searchedQuiets[i].pieceType, searchedQuiets[i].to)],
                            malus, MAX_HISTORY);
                    }
                }

                // Capture history malus: penalize captures searched before the cutoff move.
                const int capMalusEnd = wasCapture ? numSearchedCaptures - 1 : numSearchedCaptures;
                for (int i = 0; i < capMalusEnd; ++i) {
                    applyHistoryGravity(runtime.captureHistory[usSide][searchedCaptures[i].to][searchedCaptures[i].victimType][0],
                                        malus, MAX_CAPTURE_HISTORY);
                    applyHistoryGravity(runtime.captureHistory[usSide][searchedCaptures[i].to][searchedCaptures[i].victimType][1],
                                        malus, MAX_CAPTURE_HISTORY);
                }
            }
            break;
        }
    }

    if (!searchedAnyMove && runtime.isInterrupted()) {
        return SearchMoveResult{bestMove, ctx.staticEval};
    }

    return SearchMoveResult{bestMove, best};
}

int32_t Searcher::searchPosition(
    chess::Board& b,
    SearchRuntime& runtime,
    int32_t depth,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool useTT,
    bool allowTTWrite,
    bool allowHeuristicUpdates,
    const chess::Board::Move* previousMove,
    uint64_t* nodeCounter,
    bool allowNullMove,
    chess::Board::Move excludedMove) noexcept {

    const bool hasExcludedMove = excludedMove.from.isValid();

    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &runtime.nodesSearched;
    ++(*counter);

    // UCI `go nodes` hard cap. runtime.nodesSearched holds prior IDS iterations;
    // *counter is the current iteration / worker. Sum is the total node budget used.
    if (runtime.maxNodes > 0 && runtime.nodesSearched + *counter >= runtime.maxNodes) [[unlikely]] {
        runtime.markInterrupted();
        return Evaluator::evaluate(b);
    }

    int32_t earlyScore = 0;
    if (checkEarlyTerminalConditions(b, runtime, ply, earlyScore)) {
        return earlyScore;
    }

    const bool isPVNode = (static_cast<int64_t>(beta) - static_cast<int64_t>(alpha) > 1);

    if (ply > 0) {
        alpha = std::max(alpha, NEG_INF + ply);
        beta  = std::min(beta,  POS_INF - ply);
        if (alpha >= beta) return alpha;
    }

    // Actual draw terminals must be recognized before qsearch. Otherwise a
    // horizon node that completes a repetition is scored by static material.
    int32_t drawScore = 0;
    if (checkDrawTerminalConditions(b, drawScore)) {
        return drawScore;
    }

    if (depth <= 0) {
        return quiescenceSearch(b, runtime, alpha, beta, ply, useTT, counter, allowTTWrite);
    }

    // TB WDL probe (in-search): only return Draw as exact. Returning Win or
    // Loss anywhere — even guarded by alpha/beta — collapses move ordering:
    // every TB-winning subtree at root produces the same TB_WIN_SCORE-ply
    // score because aspiration windows around small eval-derived scores trip
    // the alpha/beta cutoff at every TB-Loss child node, and PVS scout then
    // never finds a "better" move than moves[0]. The king-wanders-instead-of-
    // pushing-pawns behaviour in KPPvK comes from exactly this.
    //
    // Skipping Win/Loss here costs us TB-based pruning inside the search
    // (the search will work harder in TB-known subtrees), but the static
    // evaluator can now differentiate between equally-winning moves, which
    // is what we need when Pyrrhic ranks every guaranteed win identically.
    // The root TB probe still handles the decisive Win/Loss choice.
    if (runtime.syzygyProber != nullptr
        && runtime.syzygyProber->isLoaded()
        && depth >= runtime.syzygyProber->probeDepth
        && runtime.syzygyProber->inTBRange(b)) {
        if (const auto wdl = runtime.syzygyProber->probeWDL(b)) {
            const int32_t tbScore = syzygy::SyzygyProber::wdlToScore(*wdl, ply);
            if (tbScore == 0) {
                // Draw: exact terminal — prevents picking a drawn move when
                // a winning one exists (and vice versa).
                if (runtime.transpositionTable != nullptr) {
                    runtime.transpositionTable->store(
                        b.getHash(), static_cast<uint8_t>(depth),
                        tbScore, TranspositionTable::Entry::EXACT);
                }
                return tbScore;
            }
            // Win/Loss: deliberately fall through (no cutoff, no bound store).
        }
    }

    const uint64_t hashKey = b.getHash();
    AlphaBeta bounds{alpha, beta};
    int32_t score = 0;
    const bool canUseTT = useTT && (runtime.transpositionTable != nullptr);
    if (canUseTT && !hasExcludedMove && handleSearchPrelude(runtime, depth, bounds, score, hashKey, ply)) {
        return score;
    }
    if (hasExcludedMove) allowTTWrite = false;

    SearchNodeState node{};
    node.activeColor = b.getActiveColor();
    node.inCheck = b.inCheck(node.activeColor);
    node.isPVNode = isPVNode;
    const int nonPawnMajorsAll = b.getIncrementalNonPawnMajorCount();
    node.isPawnEndgameForPruning =
        ((b.pawns_bb[0] | b.pawns_bb[1]) != 0ULL) && (nonPawnMajorsAll <= 4);

    // Compute static eval at every ply (including the root) so that the
    // `improving` heuristic can compare evalStack[ply-2] vs current eval
    // starting from ply >= 2. Previously the root left evalStack[0] at its
    // zero-initialised value, biasing `improving` to (staticEval > 0) at ply 2.
    if (!node.inCheck) {
        node.staticEval = Evaluator::evaluate(b);
        if (canUseTT) {
            int32_t ttStaticScore = 0;
            uint8_t ttStaticFlag = 0;
            if (runtime.transpositionTable->probeSE(hashKey, 0, ttStaticScore, ttStaticFlag)) {
                ttStaticScore = scoreFromTT(ttStaticScore, ply); // re-base mate scores
                // Negamax (STM-relative): a LOWERBOUND tightens upward, an
                // UPPERBOUND tightens downward, EXACT always replaces.
                if (ttStaticFlag == TranspositionTable::Entry::EXACT
                    || (ttStaticFlag == TranspositionTable::Entry::LOWERBOUND && ttStaticScore > node.staticEval)
                    || (ttStaticFlag == TranspositionTable::Entry::UPPERBOUND && ttStaticScore < node.staticEval)) {
                    node.staticEval = ttStaticScore;
                }
            }
        }
        // Nudge the static eval by the learned correction-history signal.
        node.staticEval = std::clamp(node.staticEval + evalCorrection(runtime, b),
                                     -MATE_BOUND + 1, MATE_BOUND - 1);
    }

    // Per-thread (Lazy SMP): a shared evalStack would race and corrupt the
    // `improving` prune. Each ancestor writes evalStack[ply] (line below)
    // before its grandchild 2 plies down reads it on the same thread's stack,
    // so a thread_local array (no per-search reset needed) is correct.
    static thread_local int32_t evalStack[MAX_PLY] = {};

    // Store staticEval in ply stack and compute improving flag.
    // In-check nodes have no meaningful static eval, so store a sentinel
    // instead of the stale default 0, which would otherwise corrupt the
    // improving comparison two plies deeper.
    if (ply >= 0 && ply < MAX_PLY)
        evalStack[ply] = node.inCheck ? NEG_INF : node.staticEval;
    // Negamax: staticEval is side-to-move relative and ply-2 is the same
    // side, so "improving" is simply eval rising versus two plies ago.
    const bool improving = !node.inCheck && ply >= 2
        && evalStack[ply - 2] != NEG_INF
        && (node.staticEval > evalStack[ply - 2]);

    const int side = chess::Board::colorToIndex(node.activeColor);
    const int nonPawnMajors = std::popcount(
        b.knights_bb[side] | b.bishops_bb[side] |
        b.rooks_bb[side]   | b.queens_bb[side]);
    int singularExtension = 0;
    if (!hasExcludedMove && !node.inCheck && depth >= SE_MIN_DEPTH && ply > 0) {
        int32_t ttSeScore = 0;
        uint8_t ttSeFlag  = 0;
        uint16_t encodedHashMove = 0;
        if (canUseTT
            && runtime.transpositionTable->probeSE(hashKey, static_cast<uint8_t>(depth - SE_DEPTH_MARGIN), ttSeScore, ttSeFlag)
            && (ttSeFlag == TranspositionTable::Entry::LOWERBOUND || ttSeFlag == TranspositionTable::Entry::EXACT)
            && std::abs(ttSeScore) < MATE_BOUND
            && runtime.transpositionTable->probeMove(hashKey, encodedHashMove)) {

            const auto hashMove = TranspositionTable::Entry::decodeMove(encodedHashMove);

            if (hashMove.from < 64 && hashMove.to < 64) {
                chess::Board::Move seExcluded;
                seExcluded.from = chess::Coords{hashMove.from};
                seExcluded.to   = chess::Coords{hashMove.to};
                seExcluded.promotionPiece = hashMove.promo;

                ttSeScore = scoreFromTT(ttSeScore, ply);
                const int32_t seBeta = ttSeScore - SE_BETA_MARGIN * depth;

                const int32_t seScore = searchPosition(
                    b, runtime, depth / 2 - 1, seBeta - 1, seBeta, ply,
                    canUseTT, false, allowHeuristicUpdates,
                    previousMove, counter, false, seExcluded);

                if (seScore < seBeta - SE_DOUBLE_MARGIN) {
                    singularExtension = 2;
                } else if (seScore < seBeta) {
                    singularExtension = 1;
                } else if (seScore >= beta) {
                    return seScore; // multi-cut: every move beats beta, prune
                }
            }
        }
    }

    // Negamax NMP gate: allow a null move when the static eval is within
    // ~100cp of beta (giving the opponent a free move likely still fails high).
    const int32_t nmpEvalGate = node.staticEval + 100;
    const bool canNullMove = allowNullMove
        && !node.isPVNode
        && !node.inCheck
        && ply > 0
        && depth >= 4
        && nonPawnMajors >= 2
        && isBetaCutoff(nmpEvalGate, beta);

    if (canNullMove
        && tryNullMovePruning(b, node, runtime, depth, alpha, beta, ply,
                              canUseTT, allowTTWrite, allowHeuristicUpdates,
                              counter, score)) {
        return score;
    }

    const bool canReverseFutilityPrune =
        !node.isPVNode && !node.inCheck && !node.isPawnEndgameForPruning && ply > 0 && depth <= 3;
    if (canReverseFutilityPrune
        && tryReverseFutilityPruning(b, node, depth, beta, score)) {
        return score;
    }

    // Probcut (negamax): if a winning capture, searched shallow with a null
    // window above beta+margin, still beats beta+margin, this node likely
    // fails high. SEE is side-to-move relative so the filter is one-sided.
    if (!node.isPVNode && !node.inCheck && depth >= PROBCUT_MIN_DEPTH && ply > 0
        && std::abs(beta) < POS_INF - 1000) {
        const int32_t probcutBound = saturatingAdd32(beta, PROBCUT_MARGIN);
        const int32_t pcAlpha = probcutBound - 1;
        const int32_t pcBeta  = probcutBound;
        MoveList captures = engine::MoveGenerator::generateTacticalMoves(b);
        for (int i = 0; i < captures.size; ++i) {
            const auto& mc = captures[i];
            const int32_t see = Sorter::staticExchangeEvaluationPublic(b, mc);
            if (see < PROBCUT_MARGIN) continue;
            chess::Board::MoveState pcState;
            b.doMove(mc, pcState);
            // Negamax child: negate result and swap/negate the scout window.
            const int32_t pcScore = -searchPosition(b, runtime, depth - 4, -pcBeta, -pcAlpha,
                ply + 1, useTT, allowTTWrite, false, &mc, counter, false);
            b.undoMove(mc, pcState);
            if (pcScore >= probcutBound) return beta;
        }
    }

    const int prevSide = chess::Board::colorToIndex(node.activeColor) ^ 1;
    int16_t* contHistEntry = nullptr;
    if (previousMove != nullptr && previousMove->to.index < 64) {
        const int prevPiece = b.get(previousMove->to.index) & chess::Board::MASK_PIECE_TYPE;
        contHistEntry = &runtime.contHist[prevSide][prevPiece][previousMove->to.index][0][0];
    }

    SearchContext ctx{
        previousMove, counter, contHistEntry,
        depth, ply, node.staticEval, singularExtension, excludedMove,
        node.activeColor, node.inCheck, node.isPVNode, false, improving
    };

    const bool nodeInDoubleCheck = node.inCheck && b.isDoubleCheck(node.activeColor);
    MoveList moves = node.inCheck
        ? engine::MoveGenerator::generateLegalEvasions(b, true, nodeInDoubleCheck)
        : engine::MoveGenerator::generateLegalMoves(b, /*knownNotInCheck=*/true);
    if (moves.is_empty()) {
        const int32_t mdW = b.getIncrementalMaterialDelta();
        return node.inCheck
            ? (NEG_INF + ply)  // side to move is checkmated (negamax)
            : stalemateScoreFromMaterialDelta((node.activeColor == chess::Board::WHITE) ? mdW : -mdW);
    }

    bool hasHashMove = false;

    MovePicker movePicker = Sorter::sortLegalMoves(
        std::move(moves),
        ply,
        b,
        runtime,
        canUseTT ? runtime.transpositionTable : nullptr,
        ctx.previousMove,
        &hasHashMove,
        ctx.contHistEntry);

    if (!hasHashMove) {
        ctx.singularExtension = 0;  // SE requires a verified TT hash move
        if (depth >= 6 && ply > 0) ctx.iirActive = true;
    }

    const int32_t alphaOrig = bounds.alpha;
    const int32_t betaOrig = bounds.beta;

    SearchMoveResult result = searchMoves(
        b, movePicker, ctx, bounds, runtime, canUseTT, allowHeuristicUpdates, allowTTWrite);
    const int32_t best = result.score;

    if (runtime.isInterrupted()) {
        return Evaluator::evaluate(b);
    }

    // Pawn correction history: learn the (search - corrected static eval) residual,
    // but only from TRUSTWORTHY nodes — deep enough, quiet best move, non-mate, and
    // the search must genuinely contradict the static eval rather than merely confirm
    // a cutoff bound. Deeper nodes weigh more; shallow noise is suppressed.
    const bool corrLearn = (best > node.staticEval)
                        || (best < node.staticEval && best < betaOrig);
    if (corrLearn && !node.inCheck && !hasExcludedMove && depth >= 3
        && std::abs(best) < MATE_BOUND && result.move.from.isValid()
        && (b.get(result.move.to.index) & chess::Board::MASK_PIECE_TYPE) == chess::Board::EMPTY) {
        const int corrSide = chess::Board::colorToIndex(node.activeColor);
        const int32_t residual = std::clamp(best - node.staticEval, -CORR_HIST_LIMIT, CORR_HIST_LIMIT);
        const int32_t w = std::min<int32_t>(depth, CORR_HIST_MAX_W);
        auto blendCorr = [&](int16_t& cell) noexcept {
            cell = static_cast<int16_t>((cell * (CORR_HIST_BLEND - w) + residual * w) / CORR_HIST_BLEND);
        };
        blendCorr(runtime.pawnCorrHist[corrSide][pawnCorrIndex(b)]);
        blendCorr(runtime.minorCorrHist[corrSide][minorCorrIndex(b)]);
        blendCorr(runtime.majorCorrHist[corrSide][majorCorrIndex(b)]);
    }

    if (canUseTT && allowTTWrite) {
        writeTT(runtime, hashKey, ctx.depth, best, alphaOrig, betaOrig, ctx.ply, result.move);
    }

    return best;
}

int32_t Searcher::quiescenceSearch(
    chess::Board& b,
    SearchRuntime& runtime,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool useTT,
    uint64_t* nodeCounter,
    bool allowTTWrite) noexcept {
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &runtime.nodesSearched;
    ++(*counter);

    // UCI `go nodes` hard cap (same accounting as searchPosition).
    if (runtime.maxNodes > 0 && runtime.nodesSearched + *counter >= runtime.maxNodes) [[unlikely]] {
        runtime.markInterrupted();
        return Evaluator::evaluate(b);
    }

    int32_t earlyScore = 0;
    if (checkEarlyTerminalConditions(b, runtime, ply, earlyScore)) {
        return earlyScore;
    }

    int32_t drawScore = 0;
    if (checkDrawTerminalConditions(b, drawScore)) {
        return drawScore;
    }

    const bool canUseTT = useTT && (runtime.transpositionTable != nullptr);
    if (canUseTT) {
        const uint64_t hashKey = b.getHash();
        int32_t ttScore = 0;
        if (runtime.transpositionTable->probe(hashKey, 0, alpha, beta, ttScore)) {
            return scoreFromTT(ttScore, ply);
        }
    }

    const uint8_t activeColor = b.getActiveColor();
    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    const bool inCheck = b.inCheck(activeColor);
    const bool inDoubleCheck = inCheck && b.isDoubleCheck(activeColor);

    if (ply >= MAX_QSEARCH_DEPTH) {
        if (inCheck) {
            MoveList evasions = engine::MoveGenerator::generateLegalEvasions(
                b, true, inDoubleCheck);
            if (evasions.is_empty()) {
                return NEG_INF + ply; // side to move is checkmated (negamax)
            }
        }
        return Evaluator::evaluate(b);
    }

    MovePicker movePicker;
    int32_t best;
    const int32_t alphaOrig = alpha;
    const int32_t betaOrig = beta;

    if (inCheck) {
        movePicker = engine::MoveGenerator::generateQSearchEvasions(b, true, inDoubleCheck);
        if (!movePicker.hasNext()) {
            return NEG_INF + ply; // side to move is checkmated (negamax)
        }
        best = NEG_INF;
    } else {
        const int32_t standPat = Evaluator::evaluate(b) + evalCorrection(runtime, b);
        if (isBetaCutoff(standPat, beta)) {
            return standPat; // fail-soft: tighter bound than a flat beta
        }
        updateBound(standPat, alpha);

        const int32_t EARLY_DELTA_MARGIN = QUEEN_VALUE + 50;
        if (shouldDeltaPrune(standPat, EARLY_DELTA_MARGIN, alpha)) {
            return alpha; // negamax delta-prune fail-low
        }

        int32_t deltaMargin = QUEEN_VALUE;
        const int side = chess::Board::colorToIndex(activeColor);
        const uint64_t ourPawns = b.pawns_bb[side];
        
        const uint64_t nearPromoPawns = usIsWhite
            ? (ourPawns & WHITE_NEAR_PROMO_PAWNS)
            : (ourPawns & BLACK_NEAR_PROMO_PAWNS);
        if (nearPromoPawns) {
            deltaMargin += QSEARCH_PAWN_PROMO_DELTA;
        }

        // standPat is already side-to-move relative (negamax eval).
        if (standPat < QSEARCH_MATERIAL_BAD) {
            deltaMargin += QSEARCH_MATERIAL_BAD_DELTA;
        } else if (standPat < QSEARCH_MATERIAL_WORSE) {
            deltaMargin += QSEARCH_MATERIAL_WORSE_DELTA;
        }

        const int runtimeDepth = runtime.depth;
        const int qsearchDepth = std::max(0, ply - runtimeDepth);
        if (qsearchDepth > QSEARCH_DEPTH_REDUCTION_THRESHOLD) {
            deltaMargin -= QSEARCH_DEPTH_REDUCTION_PER_5 * ((qsearchDepth - QSEARCH_DEPTH_REDUCTION_THRESHOLD) / 5);
            deltaMargin = std::max(deltaMargin, QSEARCH_DELTAMARGIN_MIN);
        }

        if (shouldDeltaPrune(standPat, deltaMargin, alpha)) {
            return alpha; // negamax delta-prune fail-low
        }

        movePicker = engine::MoveGenerator::generateQSearchTacticalMoves(
            b, standPat, alpha, ply);
        if (!movePicker.hasNext()) {
            return standPat;
        }
        best = standPat;
    }

    while (movePicker.hasNext()) {
        const auto m = movePicker.nextMove();

        if (runtime.shouldAbort()) {
            runtime.markInterrupted();
            return Evaluator::evaluate(b);
        }

        const int fromPiece = b.get(m.from.index);
        const int pieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;
        if (!inCheck && pieceType == chess::Board::KING
            && !b.isLegalPseudoMove(m.from.index, m.to.index, fromPiece))
            continue;

        chess::Board::MoveState state;
        b.doMove(m, state);
        // Negamax: child is opponent to move -> negate + swap/negate window.
        const int32_t score = -quiescenceSearch(b, runtime, -beta, -alpha, ply + 1, canUseTT, counter, allowTTWrite);
        b.undoMove(m, state);

        if (isBetter(score, best)) {
            best = score;
        }

        updateBound(score, alpha);
        if (isBetaCutoff(score, beta)) {
            if (!inCheck && canUseTT && allowTTWrite) {
                // Store `best` so the stored score matches the flag derived
                // from it (consistent with the fall-through store below);
                // storing the raw cutoff bound was looser and inconsistent.
                writeTT(runtime, b.getHash(), 0, best, alphaOrig, betaOrig, ply);
            }
            return best; // fail-soft: best >= score >= beta
        }
    }

    if (!inCheck && canUseTT && allowTTWrite) {
        writeTT(runtime, b.getHash(), 0, best, alphaOrig, betaOrig, ply);
    }

    return best;
}

chess::Board::Move Searcher::getBestMove(
    chess::Board& rootBoard,
    const MoveList& moves,
    SearchRuntime& runtime,
    int32_t alpha,
    int32_t beta) noexcept {
    int32_t bestScore = NEG_INF;
    chess::Board::Move bestMove = moves[0];
    uint64_t localNodes = 0;
    bool searchedAnyMove = false;

    MovePicker orderedRootMoves = Sorter::sortLegalMoves(
        moves,
        0,
        rootBoard,
        runtime,
        runtime.transpositionTable,
        nullptr,
        nullptr);
    
    // YBWC and Root iterations require fully sorted list upfront.
    orderedRootMoves.fullSort();
    
    const MoveList& rootMoves = orderedRootMoves.moves;

    const bool useYBWC = (rootMoves.size >= YBWC_MIN_MOVES
        && runtime.depth >= YBWC_MIN_DEPTH);

    if (!useYBWC) {
        // First move uses the full PVS window with no scout/research; the rest
        // use a null window plus research. Kept as one loop with isFirst gating;
        // the first move deliberately skips the interrupt/beta-cutoff break so
        // search-tree shape (and node count) is unchanged from the prior split.
        for (int i = 0; i < rootMoves.size; ++i) {
            if (runtime.shouldAbort()) {
                runtime.markInterrupted();
                break;
            }

            const auto& m = rootMoves[i];
            const bool isFirst = (i == 0);

            int32_t score;
            if (isFirst) {
                score = searchRootMoveScore(rootBoard, m, runtime, alpha, beta, true, true, &localNodes);
            } else {
                const int32_t nullBeta = saturatingAdd32(alpha, 1);
                score = searchRootMoveScore(rootBoard, m, runtime, alpha, nullBeta, true, true, &localNodes);
                if (shouldResearchPVS(score, alpha)) {
                    score = searchRootMoveScore(rootBoard, m, runtime, alpha, beta, true, true, &localNodes);
                }
            }

            searchedAnyMove = true;
            if (!isFirst && runtime.isInterrupted()) {
                break;
            }

            updateMinMax(score, alpha, bestScore, bestMove, m);
            if (!isFirst && isBetaCutoff(bestScore, beta)) break;
        }

        runtime.nodesSearched += localNodes;
        if (searchedAnyMove) runtime.eval = bestScore;
        return bestMove;
    }
    {
        const auto& firstMove = rootMoves[0];
        const int32_t score = searchRootMoveScore(rootBoard, firstMove, runtime, alpha, beta, true, true, &localNodes);
        searchedAnyMove = true;
        updateMinMax(score, alpha, bestScore, bestMove, firstMove);
    }

    if (runtime.isInterrupted() || rootMoves.size <= 1) [[unlikely]] {
        runtime.nodesSearched += localNodes;
        runtime.eval = bestScore;
        return bestMove;
    }

    std::array<int32_t, MAX_MOVES> threadScores;
    threadScores.fill(NEG_INF);
    std::array<uint64_t, MAX_MOVES> threadNodeCounts {};
    std::array<uint8_t, MAX_MOVES> threadNeedsResearch {};

    int candidateThreads = std::max(1, rootMoves.size - 1);
    const int threadsToUse = std::max(1, std::min(runtime.maxThreads, candidateThreads));

    auto searchDeferredRootMove = [&](int i, chess::Board& threadBoard) noexcept {
        const auto m = rootMoves[i];
        uint64_t workerNodes = 0;
        const int32_t nullBeta = saturatingAdd32(alpha, 1);
        // allowTTWrite=true: deferred root subtrees memoise their own LMR
        // re-searches (which otherwise re-expand from scratch, since the PVS
        // scout and its full-depth re-search hit no shared entries).
        const int32_t score = searchRootMoveScore(
            threadBoard, m, runtime, alpha, nullBeta, true, false, &workerNodes);

        threadScores[i] = score;
        threadNodeCounts[i] = workerNodes;
        threadNeedsResearch[i] = shouldResearchPVS(score, alpha);
    };

    if (threadsToUse <= 1) {
        for (int i = 1; i < rootMoves.size; ++i) {
            if (runtime.shouldAbort()) {
                runtime.markInterrupted();
                break;
            }

            chess::Board threadBoard = rootBoard;
            searchDeferredRootMove(i, threadBoard);
            if (runtime.isInterrupted()) {
                break;
            }
        }
    } else {
        const int totalJobs = rootMoves.size - 1;
        int estimatedChunk = std::max(1, totalJobs / (threadsToUse * 4));
        const int chunk = std::min(16, estimatedChunk);

        #pragma omp parallel num_threads(threadsToUse)
        {
            #pragma omp single nowait
            {
                #pragma omp taskgroup
                {
                    for (int start = 1; start <= totalJobs; start += chunk) {
                        const int end = std::min(start + chunk, rootMoves.size);
                        #pragma omp task firstprivate(start, end)
                        {
                            if (!runtime.shouldAbort()) {
                                chess::Board threadBoard = rootBoard;
                                for (int i = start; i < end; ++i) {
                                    if (runtime.shouldAbort()) {
                                        break;
                                    }

                                    searchDeferredRootMove(i, threadBoard);
                                    if (runtime.isInterrupted()) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (int i = 1; i < rootMoves.size; ++i) {
        if (runtime.isInterrupted()) {
            break;
        }
        if (threadNodeCounts[i] == 0) continue;

        const auto& m = rootMoves[i];
        int32_t score = threadScores[i];
        if (threadNeedsResearch[i] != 0U) {
            uint64_t researchNodes = 0;
            score = searchRootMoveScore(rootBoard, m, runtime, alpha, beta, true, true, &researchNodes);
            localNodes += researchNodes;
            if (runtime.isInterrupted()) {
                break;
            }
        }

        updateMinMax(score, alpha, bestScore, bestMove, m);
        searchedAnyMove = true;

        if (isBetaCutoff(bestScore, beta)) {
            break;
        }
    }

    localNodes = std::accumulate(threadNodeCounts.begin() + 1, threadNodeCounts.begin() + rootMoves.size, localNodes);
    runtime.nodesSearched += localNodes;
    runtime.eval = bestScore;
    return bestMove;
}

void Searcher::storeRootHashMove(
    const chess::Board& rootBoard,
    const chess::Board::Move& move,
    uint64_t depth,
    int32_t score,
    SearchRuntime& runtime,
    uint8_t flag) noexcept {
    if (runtime.transpositionTable == nullptr) {
        return;
    }

    if (!chess::Coords::isInBounds(move.from) || !chess::Coords::isInBounds(move.to)) {
        return;
    }

    if (flag != TranspositionTable::Entry::EXACT
        && flag != TranspositionTable::Entry::LOWERBOUND
        && flag != TranspositionTable::Entry::UPPERBOUND) {
        flag = TranspositionTable::Entry::EXACT;
    }

    const uint16_t encodedMove = TranspositionTable::Entry::encodeMove(
        move.from.index, move.to.index, move.promotionPiece);
    runtime.transpositionTable->store(rootBoard.getHash(), depth, static_cast<int32_t>(std::clamp<int64_t>(scoreToTT(score, 0), NEG_INF, POS_INF)), flag, encodedMove);
}

namespace {

// Reconstruct a principal variation by following best moves stored in the TT.
// Each candidate is validated against the legal move list (a TT key collision
// could otherwise yield an illegal move), and the walk stops on a repetition so
// a cyclic PV cannot loop forever. The board is left exactly as it was found.
std::string buildPvFromTT(chess::Board& board, const TranspositionTable* tt, int maxLen) noexcept {
    if (tt == nullptr) return {};

    std::array<chess::Board::Move, MAX_PLY> pvMoves{};
    std::array<chess::Board::MoveState, MAX_PLY> states{};
    int applied = 0;
    std::string pv;

    for (int i = 0; i < maxLen && applied < static_cast<int>(MAX_PLY); ++i) {
        uint16_t encoded = 0;
        if (!tt->probeMove(board.getHash(), encoded) || encoded == 0) break;

        const auto decoded = TranspositionTable::Entry::decodeMove(encoded);
        const chess::Board::Move mv{chess::Coords{decoded.from}, chess::Coords{decoded.to}, decoded.promo};

        const MoveList legal = engine::MoveGenerator::generateLegalMoves(board);
        bool legalMove = false;
        for (int k = 0; k < legal.size; ++k) {
            if (legal[k] == mv) { legalMove = true; break; }
        }
        if (!legalMove) break;

        if (!pv.empty()) pv += ' ';
        pv += mv.toUCIString();

        pvMoves[applied] = mv;
        board.doMove(mv, states[applied]);
        ++applied;

        if (board.countRepetitions() >= 3) break;
    }

    for (int i = applied - 1; i >= 0; --i) {
        board.undoMove(pvMoves[i], states[i]);
    }
    return pv;
}

// Emit one UCI `info` line for a completed iteration: depth, score (cp or
// mate N), node/time/nps counters and a TT-reconstructed PV.
void emitUciInfoLine(uint64_t depth, int32_t score, const SearchRuntime& runtime,
                     chess::Board& rootBoard) noexcept {
    const int64_t timeMs = (runtime.timeManager != nullptr)
        ? runtime.timeManager->elapsedMs() : 0;
    const uint64_t nodes = runtime.nodesSearched;
    const int64_t nps = (timeMs > 0)
        ? static_cast<int64_t>(static_cast<long double>(nodes) * 1000.0L / static_cast<long double>(timeMs))
        : 0;

    std::cout << "info depth " << depth << " score ";
    const int32_t absScore = (score < 0) ? -score : score;
    if (absScore >= MATE_BOUND) {
        const int32_t plies = Searcher::POS_INF - absScore;
        const int32_t mateMoves = (plies + 1) / 2;
        std::cout << "mate " << (score > 0 ? mateMoves : -mateMoves);
    } else {
        std::cout << "cp " << score;
    }
    std::cout << " nodes " << nodes << " nps " << nps << " time " << timeMs;

    const std::string pv = buildPvFromTT(rootBoard, runtime.transpositionTable, 16);
    if (!pv.empty()) std::cout << " pv " << pv;
    std::cout << '\n';
    std::cout.flush();
}

} // namespace

Searcher::IterativeSearchResult Searcher::runIterativeDeepening(
    chess::Board& rootBoard,
    SearchRuntime& runtime,
    uint64_t startDepth,
    uint64_t targetDepth) noexcept {
    
    IterativeSearchResult result;
    const uint64_t firstDepth = std::max<uint64_t>(1, startDepth);
    const uint64_t maxDepth = std::max<uint64_t>(firstDepth, targetDepth);

    if (rootBoard.kings_bb[0] == 0 || rootBoard.kings_bb[1] == 0) {
        const bool rootWhite = (rootBoard.getActiveColor() == chess::Board::WHITE);
        const bool ourKingGone = (rootBoard.kings_bb[0] == 0) ? rootWhite : !rootWhite;
        result.terminalRoot = true;
        result.completedAnyDepth = true;
        result.bestScore = ourKingGone ? NEG_INF : POS_INF; // STM-relative
        runtime.eval = result.bestScore;
        return result;
    }

    int32_t rootDrawScore = 0;
    if (checkDrawTerminalConditions(rootBoard, rootDrawScore, /*atRoot=*/true)) {
        MoveList drawMoves = engine::MoveGenerator::generateLegalMoves(rootBoard);
        result.terminalRoot = true;
        result.completedAnyDepth = true;
        result.bestScore = rootDrawScore;
        result.bestMove = drawMoves.is_empty() ? chess::Board::Move{} : drawMoves[0];
        runtime.eval = rootDrawScore;
        return result;
    }

    MoveList moves = engine::MoveGenerator::generateLegalMoves(rootBoard);
    if (moves.is_empty()) {
        const uint8_t toMove = rootBoard.getActiveColor();
        if (rootBoard.isCheckmate(toMove)) {
            result.bestScore = NEG_INF; // side to move is mated (negamax/STM)
        } else if (rootBoard.isDraw(toMove)) {
            result.bestScore = 0;
        } else {
            result.bestScore = Evaluator::evaluate(rootBoard);
        }
        runtime.eval = result.bestScore;
        return result;
    }

    result.hasLegalMoves = true;

    // TB root probe: probeRoot returns moves with rank derived from actual
    // DTZ — higher rank is faster win / slower loss / preserved draw. The
    // optimal play in any TB-known endgame is just to follow that ranking,
    // so return immediately for all three outcomes without running a search.
    // Searching adds nothing here: TB already proves the result and orders
    // moves by exact distance-to-zeroing, while the heuristic evaluator can
    // (and does) prefer a slower converting move over the optimal one.
    //
    // Sanity check: only play the TB move if it appears in HydraY's own
    // legal-move list. If somehow it doesn't (encoding mismatch, etc.),
    // skip the TB branch and let the normal search take over.
    if (runtime.syzygyProber != nullptr && runtime.syzygyProber->isLoaded()
        && runtime.syzygyProber->inTBRange(rootBoard)) {
        const auto tbMoves = runtime.syzygyProber->probeRoot(rootBoard);
        if (!tbMoves.empty()) {
            int32_t bestRank = tbMoves[0].tbRank;
            chess::Board::Move tbBest = tbMoves[0].move;
            for (const auto& tm : tbMoves) {
                if (tm.tbRank > bestRank) { bestRank = tm.tbRank; tbBest = tm.move; }
            }
            bool tbBestIsLegal = false;
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i] == tbBest) { tbBestIsLegal = true; break; }
            }
            if (tbBestIsLegal) {
                const int32_t tbScore = syzygy::SyzygyProber::wdlToScore(
                    bestRank > 0 ? syzygy::WDL::Win  :
                    bestRank < 0 ? syzygy::WDL::Loss :
                                   syzygy::WDL::Draw, 0);
                result.completedAnyDepth = true;
                result.bestMove  = tbBest;
                result.bestScore = tbScore;
                runtime.eval     = tbScore;
                std::cout << "info depth 1 score cp " << tbScore
                          << " tbrank " << bestRank
                          << " pv " << tbBest.toUCIString() << "\n";
                return result;
            }
            // Fall through to search if the TB move isn't legal in our move list.
        }
    }

    chess::Board::Move bestMove = moves[0];
    int32_t prevPrevScore = 0;
    int32_t prevScore = 0;
    bool hasPrevScore = false;
    bool hasPrevPrevScore = false;
    auto absScore = [](int32_t v) noexcept -> int32_t {
        if (v == NEG_INF) return POS_INF;
        return (v >= 0) ? v : -v;
    };

    for (uint64_t currentDepth = firstDepth; currentDepth <= maxDepth; ++currentDepth) {
        if (runtime.shouldAbort()) {
            break;
        }

        // Soft limit: once a move is in hand, do not open a depth we almost
        // certainly cannot finish within the time budget.
        if (result.completedAnyDepth && runtime.timeManager != nullptr
            && !runtime.timeManager->shouldStartNextDepth()) {
            break;
        }

        runtime.depth = currentDepth;

        if (result.completedAnyDepth) {
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i] == bestMove) {
                    moves.rotate(i);
                    break;
                }
            }
        }

        runtime.clearInterrupted();
        bool iterationCompleted = true;
        int32_t iterationAlpha = NEG_INF;
        int32_t iterationBeta = POS_INF;
        chess::Board::Move candidateBestMove = moves[0];

        const bool canUseAspiration =
            hasPrevScore
            && result.completedAnyDepth
            && currentDepth >= 5
            && absScore(prevScore) < MATE_BOUND;

        if (!canUseAspiration) {
            candidateBestMove = getBestMove(rootBoard, moves, runtime);
            if (runtime.isInterrupted()) {
                iterationCompleted = false;
            }
        } else {
            const int32_t swingBase = hasPrevPrevScore ? prevPrevScore : prevScore;
            const int64_t scoreDiff64 = static_cast<int64_t>(prevScore) - static_cast<int64_t>(swingBase);
            const int64_t scoreSwing64 = (scoreDiff64 >= 0) ? scoreDiff64 : -scoreDiff64;
            const int32_t scoreSwing = std::min<int64_t>(scoreSwing64, POS_INF);
            int32_t windowDelta = std::clamp<int32_t>(15 + (scoreSwing / 4), 25, 100);
            int aspirationResearches = 0;
            int32_t centerScore = prevScore;
            int32_t aspAlpha = saturatingSub32(centerScore, windowDelta);
            int32_t aspBeta = saturatingAdd32(centerScore, windowDelta);

            while (true) {
                iterationAlpha = aspAlpha;
                iterationBeta = aspBeta;
                candidateBestMove = getBestMove(rootBoard, moves, runtime, aspAlpha, aspBeta);
                if (runtime.isInterrupted()) {
                    iterationCompleted = false;
                    break;
                }

                const int32_t score = runtime.eval;
                const bool failLow = (score <= aspAlpha);
                const bool failHigh = (score >= aspBeta);
                if (!failLow && !failHigh) {
                    break;
                }

                ++aspirationResearches;
                if (failLow) {
                    centerScore = std::min(centerScore, score);
                } else {
                    centerScore = std::max(centerScore, score);
                }

                windowDelta = std::min<int32_t>(WINDOW_HARD_CAP, windowDelta * 2 + 10);
                if (aspirationResearches >= MAX_ASP_RESEARCHES || windowDelta >= WINDOW_HARD_CAP) {
                    iterationAlpha = NEG_INF;
                    iterationBeta = POS_INF;
                    candidateBestMove = getBestMove(rootBoard, moves, runtime);
                    if (runtime.isInterrupted()) {
                        iterationCompleted = false;
                    }
                    break;
                }

                if (failLow) {
                    aspAlpha = std::max<int32_t>(NEG_INF, saturatingSub32(centerScore, windowDelta));
                    aspBeta = std::min<int32_t>(POS_INF, saturatingAdd32(centerScore, std::max<int32_t>(25, windowDelta / 2)));
                } else {
                    aspAlpha = std::max<int32_t>(NEG_INF, saturatingSub32(centerScore, std::max<int32_t>(25, windowDelta / 2)));
                    aspBeta = std::min<int32_t>(POS_INF, saturatingAdd32(centerScore, windowDelta));
                }
            }
        }

        if (!iterationCompleted) {
            break;
        }

        const bool hadPrevScore = hasPrevScore;
        const int32_t prevScoreBefore = prevScore;
        const bool bestMoveChanged =
            result.completedAnyDepth && !(bestMove == candidateBestMove);

        if (hasPrevScore) {
            prevPrevScore = prevScore;
            hasPrevPrevScore = true;
        }

        bestMove = candidateBestMove;
        prevScore = runtime.eval;
        hasPrevScore = true;
        result.completedAnyDepth = true;

        if (runtime.timeManager != nullptr) {
            runtime.timeManager->updateStability(
                bestMoveChanged, runtime.eval, prevScoreBefore, hadPrevScore);
        }
        result.completedDepth = currentDepth;
        result.bestMove = bestMove;
        result.bestScore = runtime.eval;
        result.rootScoreBound = determineFlag(result.bestScore, iterationAlpha, iterationBeta);
        storeRootHashMove(rootBoard, bestMove, currentDepth, runtime.eval, runtime, static_cast<uint8_t>(result.rootScoreBound));

        if (runtime.emitUciInfo) {
            emitUciInfoLine(currentDepth, runtime.eval, runtime, rootBoard);
        }
    }

    return result;
}

} // namespace engine
