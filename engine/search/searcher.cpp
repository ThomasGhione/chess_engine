#include "searcher.hpp"

#include <cmath>
#include <algorithm>
#include <numeric>

#include "../engine.hpp"
#include "../eval/evaluator.hpp"
#include "move_generator.hpp"
#include "sorter.hpp"

namespace engine {

constexpr int32_t Searcher::initialBest(bool isWhite) noexcept {
    return isWhite ? NEG_INF : POS_INF;
}

constexpr bool Searcher::isBetter(int32_t newScore, int32_t currentBest, bool isWhite) noexcept {
    return isWhite ? (newScore > currentBest) : (newScore < currentBest);
}

bool Searcher::isBetaCutoff(int32_t score, int32_t alpha, int32_t beta, bool isWhite) noexcept {
    return isWhite ? (score >= beta) : (score <= alpha);
}

void Searcher::updateBound(int32_t score, int32_t& alpha, int32_t& beta, bool isWhite) noexcept {
    if (isWhite) {
        if (score > alpha) alpha = score;
    } else {
        if (score < beta) beta = score;
    }
}

bool Searcher::shouldDeltaPrune(int32_t standPat, int32_t margin, int32_t alpha, int32_t beta, bool isWhite) noexcept {
    const int64_t standPat64 = static_cast<int64_t>(standPat);
    const int64_t margin64 = static_cast<int64_t>(margin);
    const int64_t alpha64 = static_cast<int64_t>(alpha);
    const int64_t beta64 = static_cast<int64_t>(beta);
    return isWhite ? (standPat64 + margin64 < alpha64) : (standPat64 - margin64 > beta64);
}

int32_t Searcher::cutoffValue(int32_t alpha, int32_t beta, bool isWhite) noexcept {
    return isWhite ? beta : alpha;
}

bool Searcher::shouldResearchPVS(int32_t score, int32_t alphaBound, int32_t betaBound, bool isWhite) noexcept {
    return isWhite ? (score > alphaBound) : (score < betaBound);
}

void Searcher::toTTProbeBounds(int32_t alpha, int32_t beta, int32_t& ttAlpha, int32_t& ttBeta) noexcept {
    const int64_t expandedAlpha = static_cast<int64_t>(alpha) - TranspositionTable::ADJUSTMENT;
    const int64_t expandedBeta = static_cast<int64_t>(beta) + TranspositionTable::ADJUSTMENT;
    ttAlpha = clampToInt32(expandedAlpha);
    ttBeta = clampToInt32(expandedBeta);
}

int32_t Searcher::saturatingAdd32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t sum = static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs);
    return clampToInt32(sum);
}

int32_t Searcher::saturatingSub32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t diff = static_cast<int64_t>(lhs) - static_cast<int64_t>(rhs);
    return clampToInt32(diff);
}

int16_t Searcher::clampHeuristic16(int32_t value) noexcept {
    constexpr int32_t MIN_I16 = -32768;
    constexpr int32_t MAX_I16 = 32767;
    return std::clamp(value, MIN_I16, MAX_I16);
}

bool Searcher::hasSearchStopControl(const SearchRuntime& runtime) noexcept {
    return runtime.stopSearchRequested != nullptr || runtime.ponderingStopRequested != nullptr;
}

bool Searcher::shouldAbortSearch(const SearchRuntime& runtime) noexcept {
    const bool stopRequested = runtime.stopSearchRequested != nullptr
        && runtime.stopSearchRequested->load(std::memory_order_acquire);
    const bool ponderingStopRequested = runtime.ponderingStopRequested != nullptr
        && runtime.ponderingStopRequested->load(std::memory_order_acquire);
    return stopRequested || ponderingStopRequested;
}

void Searcher::markInterrupted(SearchRuntime& runtime) noexcept {
    if (runtime.searchInterrupted != nullptr) {
        runtime.searchInterrupted->store(true, std::memory_order_relaxed);
    }
}

bool Searcher::isInterrupted(const SearchRuntime& runtime) noexcept {
    return runtime.searchInterrupted != nullptr
        && runtime.searchInterrupted->load(std::memory_order_relaxed);
}

void Searcher::clearInterrupted(SearchRuntime& runtime) noexcept {
    if (runtime.searchInterrupted != nullptr) {
        runtime.searchInterrupted->store(false, std::memory_order_relaxed);
    }
}

bool Searcher::checkEarlyTerminalConditions(
    const chess::Board& b,
    SearchRuntime& runtime,
    int ply,
    int32_t& outScore) noexcept {
    
    if (shouldAbortSearch(runtime)) {
        markInterrupted(runtime);
        outScore = Evaluator::evaluate(b);
        return true;
    }

    if (ply >= MAX_PLY - 1) {
        outScore = Evaluator::evaluate(b);
        return true;
    }

    // Terminal king-capture states are possible in this codebase's move model.
    // Resolve them immediately to avoid evaluating undefined tactical positions.
    if (b.kings_bb[0] == 0ULL) {
        outScore = NEG_INF + ply;
        return true;
    }
    if (b.kings_bb[1] == 0ULL) {
        outScore = POS_INF - ply;
        return true;
    }

    return false;
}

bool Searcher::isKillerMove(
    const chess::Board::Move& m,
    const chess::Board::Move killerMoves[2][MAX_PLY],
    int ply) noexcept {
    if (ply < 0 || ply >= MAX_PLY) [[unlikely]] return false;

    const auto& km0 = killerMoves[0][ply];
    const auto& km1 = killerMoves[1][ply];
    return (m.from.index == km0.from.index && m.to.index == km0.to.index)
        || (m.from.index == km1.from.index && m.to.index == km1.to.index);
}

bool Searcher::isPromotionMove(const chess::Board& board, const chess::Board::Move& move) noexcept {
    const uint8_t toRank = move.to.rank();
    if (toRank != 0 && toRank != 7) return false;

    const uint8_t piece = board.get(move.from);
    const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
    if (pieceType != chess::Board::PAWN) return false;

    return toRank == chess::Board::promotionRank(board.getColor(move.from) == chess::Board::WHITE);
}

bool Searcher::isEnPassantCapture(const chess::Board& board, const chess::Board::Move& move) noexcept {
    const uint8_t fromPieceType = board.get(move.from) & chess::Board::MASK_PIECE_TYPE;
    if (fromPieceType != chess::Board::PAWN) return false;
    if (board.get(move.to) != chess::Board::EMPTY) return false;
    if (chess::Board::file(move.from.index) == chess::Board::file(move.to.index)) return false;

    const chess::Coords ep = board.getEnPassant();
    return chess::Coords::isInBounds(ep) && (move.to == ep);
}

bool Searcher::doMoveWithPromotion(chess::Board& b, const chess::Board::Move& m, chess::Board::MoveState& state) noexcept {
    const bool isPromo = isPromotionMove(b, m);
    const char promoChoice = isPromo ? m.promotionPiece : '\0';
    b.doMove(m, state, promoChoice);
    return isPromo;
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

int32_t Searcher::repetitionDrawScore(const chess::Board& b) noexcept {
    const int32_t matDelta = Evaluator::getMaterialDelta(b);
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) {
        return 0;
    }

    const int32_t advantage = std::abs(matDelta);
    const int32_t scaledPenalty =
        STALEMATE_DRAW_PENALTY_MINOR + (advantage - STALEMATE_MATERIAL_THRESHOLD) / 2;
    const int32_t contempt = std::clamp<int32_t>(
        scaledPenalty, STALEMATE_DRAW_PENALTY_MINOR, STALEMATE_DRAW_PENALTY_MAJOR);
    return (matDelta > 0) ? -contempt : contempt;
}

bool Searcher::hasInsufficientMaterialDraw(const chess::Board& b) noexcept {
    const uint64_t wKnights = b.knights_bb[0];
    const uint64_t bKnights = b.knights_bb[1];
    const uint64_t wBishops = b.bishops_bb[0];
    const uint64_t bBishops = b.bishops_bb[1];
    const uint64_t wMinors = wKnights | wBishops;
    const uint64_t bMinors = bKnights | bBishops;

    if (wMinors == 0ULL && bMinors == 0ULL) {
        return true;
    }

    const int wMinorCount = __builtin_popcountll(wMinors);
    const int bMinorCount = __builtin_popcountll(bMinors);
    return (wMinorCount <= 1 && bMinorCount == 0)
        || (bMinorCount <= 1 && wMinorCount == 0);
}

void Searcher::rootNullWindow(bool usIsWhite, int32_t alpha, int32_t beta, int32_t& outAlpha, int32_t& outBeta) noexcept {
    outAlpha = usIsWhite ? alpha : saturatingSub32(beta, 1);
    outBeta = usIsWhite ? saturatingAdd32(alpha, 1) : beta;
}

void Searcher::updateMinMax(
    bool usIsWhite,
    int32_t score,
    int32_t& alpha,
    int32_t& beta,
    int32_t& bestScore,
    chess::Board::Move& bestMove,
    const chess::Board::Move& m) noexcept {
    if (isBetter(score, bestScore, usIsWhite)) {
        bestScore = score;
        bestMove = m;
    }

    updateBound(score, alpha, beta, usIsWhite);
}

void Searcher::softResetHistory(SearchRuntime& runtime) noexcept {
    // Macro-step 1: Flatten history tensor in contiguous memory.
    int16_t* historyFlat = &runtime.history[0][0][0];
    constexpr int HISTORY_CELLS = 2 * 64 * 64;

    // Macro-step 2: Apply age decay by halving every entry.
    #pragma omp simd
    for (int i = 0; i < HISTORY_CELLS; ++i) {
        historyFlat[i] >>= 1;
    }
}

chess::Board::Move Searcher::searchBestMove(
    chess::Board& board,
    SearchRuntime& runtime,
    uint64_t requestedDepth) noexcept {
    // Macro-step 1: Normalize depth request and clear stop/interruption markers.
    const uint64_t targetDepth = (requestedDepth == 0)
        ? DEFAULT_DEPTH
        : requestedDepth;
    if (targetDepth == 0) return chess::Board::Move{};

    clearInterrupted(runtime);

    // Macro-step 2: Prepare TT generation, node counter, and heuristic decay.
    if (runtime.transpositionTable != nullptr) {
        runtime.transpositionTable->incrementGeneration();
    }
    runtime.nodesSearched = 0;
    softResetHistory(runtime);

    // Macro-step 3: Run iterative deepening on the provided board.
    IterativeSearchResult result = runIterativeDeepening(board, runtime, 1, targetDepth, false);
    runtime.depth = targetDepth;

    // Macro-step 4: Return best move or deterministic fallback when interrupted/empty.
    if (!result.hasLegalMoves || !result.completedAnyDepth) {
        MoveList<chess::Board::Move> fallbackMoves = engine::MoveGenerator::generateLegalMoves(board);
        if (fallbackMoves.is_empty()) {
            runtime.eval = Evaluator::evaluate(board);
            return chess::Board::Move{};
        }
        runtime.eval = Evaluator::evaluate(board);
        return fallbackMoves[0];
    }

    runtime.eval = result.bestScore;
    return result.bestMove;
}

int32_t Searcher::searchRootMoveScore(
    chess::Board& b,
    const chess::Board::Move& m,
    SearchRuntime& runtime,
    int32_t alpha,
    int32_t beta,
    int currPly,
    bool useTT,
    bool allowTTWrite,
    bool allowHeuristicUpdates,
    uint64_t* nodeCounter) noexcept {
    chess::Board::MoveState state;
    doMoveWithPromotion(b, m, state);
    const int32_t score = searchPosition(
        b, runtime, runtime.depth - 1, alpha, beta, currPly,
        useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter);
    b.undoMove(m, state);
    return score;
}

bool Searcher::handleSearchPrelude(
    const SearchRuntime& runtime,
    int32_t depth,
    const AlphaBeta& bounds,
    int32_t& score,
    uint64_t hashKey) noexcept {
    if (runtime.transpositionTable == nullptr) {
        return false;
    }

    if (depth >= 2) runtime.transpositionTable->prefetch(hashKey);

    int32_t ttScore = 0;
    int32_t ttAlpha = 0;
    int32_t ttBeta = 0;
    toTTProbeBounds(bounds.alpha, bounds.beta, ttAlpha, ttBeta);
    if (runtime.transpositionTable->probe(hashKey, static_cast<uint8_t>(depth), ttAlpha, ttBeta, ttScore)) {
        score = ttScore;
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
    const int32_t reduction = 3 + depth / 8;

    chess::Board::MoveState nullState;
    b.doNullMove(nullState);

    const int32_t nullScore = searchPosition(
        b, runtime, depth - reduction, alpha, beta, ply + 1,
        useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter, false);

    b.undoNullMove(nullState);

    if (!isBetaCutoff(nullScore, alpha, beta, node.usIsWhite)) {
        return false;
    }

    bool confirmedCutoff = true;
    //FIXME: Elimina numero magico
    if (depth >= 10) {
        const int32_t verifyScore = searchPosition(
            b, runtime, depth - reduction, alpha, beta, ply,
            useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter, false);
        confirmedCutoff = isBetaCutoff(verifyScore, alpha, beta, node.usIsWhite);
    }

    if (!confirmedCutoff) {
        return false;
    }

    if (!b.hasAnyLegalMove(node.activeColor)) {
        outScore = stalemateScoreFromMaterialDelta(Evaluator::getMaterialDelta(b));
        return true;
    }

    outScore = cutoffValue(alpha, beta, node.usIsWhite);
    return true;
}

bool Searcher::tryReverseFutilityPruning(
    chess::Board& b,
    const SearchNodeState& node,
    int32_t depth,
    int32_t alpha,
    int32_t beta,
    int ply,
    int32_t& outScore) noexcept {
    constexpr int32_t RFP_MARGIN_PER_DEPTH = 110;

    if (node.isPVNode || node.inCheck || node.isPawnEndgameForPruning || ply <= 0 || depth > 3) {
        return false;
    }

    const int32_t rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
    const int32_t rfpScore = node.usIsWhite
        ? (node.staticEval - rfpMargin)
        : (node.staticEval + rfpMargin);
    if (!isBetaCutoff(rfpScore, alpha, beta, node.usIsWhite)) {
        return false;
    }

    if (!b.hasAnyLegalMove(node.activeColor)) {
        outScore = stalemateScoreFromMaterialDelta(Evaluator::getMaterialDelta(b));
        return true;
    }

    outScore = node.staticEval;
    return true;
}

void Searcher::updateKillerAndHistoryOnBetaCutoff(
    const chess::Board& b,
    const chess::Board::Move& m,
    int32_t depth,
    int ply,
    uint8_t us,
    SearchRuntime& runtime,
    const chess::Board::Move* previousMove) noexcept {
    if (ply < 0 || ply >= MAX_PLY) return;

    const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
    const bool isEpCapture = isEnPassantCapture(b, m);
    const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
    const uint8_t epVictimType = chess::Board::PAWN;
    const uint8_t victimType = isEpCapture ? epVictimType : toPieceType;
    const uint8_t fromIndex = m.from.index;
    const uint8_t toIndex = m.to.index;

    // CAPTURE HISTORY: bonus for captures that cause cutoffs.
    if (isCapture) {
        const int colorIndex = chess::Board::colorToIndex(us);
        const int32_t depthPlusOne = depth + 1;
        const int32_t bonus = depthPlusOne * depthPlusOne;

        constexpr int32_t MAX_CAPTURE_HISTORY = 10000;
        auto& chPrimary = runtime.captureHistory[colorIndex][toIndex][victimType][0];
        auto& chSecondary = runtime.captureHistory[colorIndex][toIndex][victimType][1];
        int32_t primaryScore = chPrimary;
        primaryScore += bonus - primaryScore * std::abs(bonus) / MAX_CAPTURE_HISTORY;
        chPrimary = clampHeuristic16(primaryScore);

        const int32_t secondaryBonus = (bonus >> 1);
        int32_t secondaryScore = chSecondary;
        secondaryScore += secondaryBonus - secondaryScore * std::abs(secondaryBonus) / MAX_CAPTURE_HISTORY;
        chSecondary = clampHeuristic16(secondaryScore);
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
    const int colorIndex = chess::Board::colorToIndex(us);
    const int32_t depthPlusOne = depth + 1;
    const int32_t bonus = depthPlusOne * depthPlusOne;

    constexpr int32_t MAX_HISTORY = 16384;
    auto& h = runtime.history[colorIndex][fromIndex][toIndex];
    int32_t historyScore = h;
    historyScore += bonus - historyScore * std::abs(bonus) / MAX_HISTORY;
    h = clampHeuristic16(historyScore);
}

Searcher::SearchMoveResult Searcher::searchMoves(
    chess::Board& b,
    MoveList<chess::Board::Move>& orderedMoves,
    int32_t* moveScores,
    bool usIsWhite,
    const SearchContext& ctx,
    AlphaBeta& bounds,
    SearchRuntime& runtime,
    bool useTT,
    bool allowHeuristicUpdates,
    bool allowTTWrite) noexcept {
    // Macro-step 1: Initialize best-score tracking and quiet-move malus buffers.
    int32_t best = initialBest(usIsWhite);
    chess::Board::Move bestMove = orderedMoves[0];
    bool searchedAnyMove = false;

    struct QuietEntry { uint8_t from; uint8_t to; };
    //FIXME: Spostare fuori costanti
    constexpr int MAX_QUIETS_TRACKED = 64;
    QuietEntry searchedQuiets[MAX_QUIETS_TRACKED];
    int numSearchedQuiets = 0;

    // Macro-step 2: Precompute pruning buckets and loop invariants.
    const int nonPawnMajorsForLMR = __builtin_popcountll(
        b.knights_bb[0] | b.knights_bb[1] |
        b.bishops_bb[0] | b.bishops_bb[1] |
        b.rooks_bb[0]   | b.rooks_bb[1]   |
        b.queens_bb[0]  | b.queens_bb[1]);
    const bool isDelicateEndgame = (nonPawnMajorsForLMR <= 2);
    const bool isLateEndgame = (nonPawnMajorsForLMR <= 5);

    static constexpr int32_t FUTILITY_MARGINS_MG[] = {0, 260, 520};
    static constexpr int32_t FUTILITY_MARGINS_EG[] = {0, 170, 350};
    const bool canPruneByDepthAndNodeType =
        !ctx.isPVNode && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;

    const bool canFutilityPruneRegular = canPruneByDepthAndNodeType && !isDelicateEndgame;
    // Delicate endgames: keep futility nearly off, but allow a tiny depth-1 gate.
    const bool canFutilityPruneDelicate = canPruneByDepthAndNodeType && isDelicateEndgame && (ctx.depth == 1);
    const bool canFutilityPrune = canFutilityPruneRegular || canFutilityPruneDelicate;
    const int32_t futilityMargin = canFutilityPruneRegular
        ? (isLateEndgame ? FUTILITY_MARGINS_EG[ctx.depth] : FUTILITY_MARGINS_MG[ctx.depth])
        : (canFutilityPruneDelicate ? 180 : 0);

    static constexpr int LMP_THRESHOLDS_MG[] = {0, 12, 20, 30};
    static constexpr int LMP_THRESHOLDS_EG[] = {0, 16, 26, 38};
    const bool canLMPRegular = canPruneByDepthAndNodeType && !isDelicateEndgame;
    // Delicate endgames: prune only very-late quiets at depth-1.
    const bool canLMPDelicate = canPruneByDepthAndNodeType && isDelicateEndgame && (ctx.depth == 1);
    const bool canLMP = canLMPRegular || canLMPDelicate;
    const int lmpThreshold = canLMPRegular
        ? (isLateEndgame ? LMP_THRESHOLDS_EG[ctx.depth] : LMP_THRESHOLDS_MG[ctx.depth])
        : (canLMPDelicate ? 48 : 999);

    const int usSide = chess::Board::colorToIndex(ctx.activeColor);
    const int oppSide = usSide ^ 1;
    const uint64_t enemyMajorOrKingTargets =
        b.rooks_bb[oppSide] | b.queens_bb[oppSide] | b.kings_bb[oppSide];
    const uint64_t enemyForkTargets =
        b.knights_bb[oppSide] | b.bishops_bb[oppSide] |
        b.rooks_bb[oppSide]   | b.queens_bb[oppSide] |
        b.kings_bb[oppSide];

    const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);

    // Macro-step 3: Incremental move picker + visit loop with LMP/futility/LMR/PVS logic.
    for (int moveIndex = 0; moveIndex < orderedMoves.size; ++moveIndex) {
        if (shouldAbortSearch(runtime)) {
            markInterrupted(runtime);
            break;
        }

        if (moveScores != nullptr) {
            // Pick best remaining move without globally sorting the full list.
            int bestIndex = moveIndex;
            int32_t pickScore = moveScores[moveIndex];
            for (int candidate = moveIndex + 1; candidate < orderedMoves.size; ++candidate) {
                if (moveScores[candidate] > pickScore) {
                    bestIndex = candidate;
                    pickScore = moveScores[candidate];
                }
            }
            if (bestIndex != moveIndex) {
                std::swap(orderedMoves[moveIndex], orderedMoves[bestIndex]);
                std::swap(moveScores[moveIndex], moveScores[bestIndex]);
            }
        }

        const chess::Board::Move m = orderedMoves[moveIndex];
        const bool isFirstMove = (moveIndex == 0);

        const bool wasCapture = (b.get(m.to) != chess::Board::EMPTY) || isEnPassantCapture(b, m);
        const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const bool isPromotionCandidate = (fromPieceType == chess::Board::PAWN)
            && (m.to.rank() == chess::Board::promotionRank(usIsWhite));
        const bool isQuietMove = !wasCapture && !isPromotionCandidate;
        const bool isQuietPawnPush = isQuietMove
            && (fromPieceType == chess::Board::PAWN)
            && (chess::Board::file(m.from.index) == chess::Board::file(m.to.index));
        const uint64_t forkTargets = pieces::PAWN_ATTACKS[usSide][m.to.index] & enemyForkTargets;
        const bool createsPawnForkThreat = isQuietPawnPush
            && ((forkTargets & enemyMajorOrKingTargets) != 0ULL)
            && (__builtin_popcountll(forkTargets) >= 2);

        if (canLMP && isQuietMove && !createsPawnForkThreat && moveIndex >= lmpThreshold) {
            continue;
        }

        const bool delicateFutilityGate = !isDelicateEndgame || (moveIndex >= 24);
        if (canFutilityPrune && delicateFutilityGate && isQuietMove && !createsPawnForkThreat && moveIndex > 0
            && shouldDeltaPrune(ctx.staticEval, futilityMargin, bounds.alpha, bounds.beta, usIsWhite)) {
            continue;
        }

        chess::Board::MoveState state;
        const bool isPromo = doMoveWithPromotion(b, m, state);

        const bool inConservativeEndgameLMR = isLateEndgame && !isDelicateEndgame;
        const int lmrMinMoveIndex = inConservativeEndgameLMR ? 14 : 12;
        const bool lmrStructuralCandidate = (ctx.depth > 6)
            && (moveIndex >= lmrMinMoveIndex)
            && !isPromo
            && (!wasCapture)
            && !createsPawnForkThreat
            && !isDelicateEndgame;
        // Delicate endgames: allow only minimal one-ply LMR on very-late quiet moves.
        const bool lmrDelicateCandidate = isDelicateEndgame
            && !ctx.isPVNode
            && !ctx.inCheck
            && (ctx.depth >= 10)
            && (moveIndex >= 30)
            && !isPromo
            && (!wasCapture)
            && !createsPawnForkThreat;

        const bool forcingCandidate = (wasCapture || isPromo || moveIndex < 3 || createsPawnForkThreat);
        const bool needsCheckInfo =
            (ctx.depth >= 2 && ctx.depth <= 4 && forcingCandidate) || lmrStructuralCandidate || lmrDelicateCandidate;
        const bool givesCheck = needsCheckInfo ? b.inCheck(oppColor) : false;

        const bool isForcingCheck = givesCheck && forcingCandidate;
        const bool shouldCheckExtend = isForcingCheck && (ctx.depth >= 2) && (ctx.depth <= 4);
        const int32_t childDepth = ctx.depth - 1 + (shouldCheckExtend ? 1 : 0);
        const bool canReduce = (lmrStructuralCandidate || lmrDelicateCandidate)
            && !givesCheck
            && !isKillerMove(m, runtime.killerMoves, ctx.ply);

        const int32_t searchAlpha = isFirstMove ? bounds.alpha : (usIsWhite ? bounds.alpha : saturatingSub32(bounds.beta, 1));
        const int32_t searchBeta  = isFirstMove ? bounds.beta  : (usIsWhite ? saturatingAdd32(bounds.alpha, 1) : bounds.beta);

	//FIXME: Trasformare in funzione helper
        int32_t score = 0;
        if (canReduce) {
            int32_t reduction = 1;
            if (lmrStructuralCandidate) {
                constexpr double LMR_C = 2.87;
                reduction = static_cast<int32_t>(std::log(static_cast<double>(ctx.depth))
                                               * std::log(static_cast<double>(moveIndex))
                                               / LMR_C);
                reduction = std::clamp(reduction, 1, ctx.depth - 3);

                if (inConservativeEndgameLMR) {
                    reduction = std::min<int32_t>(reduction, 1);
                }
            }

            const int32_t reducedDepth = std::max(1, childDepth - reduction);
            score = searchPosition(b, runtime, reducedDepth, searchAlpha, searchBeta, ctx.ply + 1,
                                   useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);

            const bool reducedFailed = shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
            if (reducedFailed && reducedDepth < childDepth) {
                score = searchPosition(b, runtime, childDepth, searchAlpha, searchBeta, ctx.ply + 1,
                                       useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
            }

            const bool shouldResearch = !isFirstMove && shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
            if (shouldResearch) {
                score = searchPosition(b, runtime, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1,
                                       useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
            }
        } else {
            score = searchPosition(b, runtime, childDepth, searchAlpha, searchBeta, ctx.ply + 1,
                                   useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);

            if (!isFirstMove) {
                const bool shouldResearch = shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
                if (shouldResearch) {
                    score = searchPosition(b, runtime, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1,
                                           useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
                }
            }
        }

        b.undoMove(m, state);
        searchedAnyMove = true;

        if (isInterrupted(runtime)) {
            break;
        }

        if (isQuietMove && numSearchedQuiets < MAX_QUIETS_TRACKED) {
            searchedQuiets[numSearchedQuiets++] = {m.from.index, m.to.index};
        }

        updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

	//FIXME: Trasformare in funzione helper
        if (isBetaCutoff(best, bounds.alpha, bounds.beta, usIsWhite)) {
            if (allowHeuristicUpdates) {
                updateKillerAndHistoryOnBetaCutoff(
                    b, m, ctx.depth, ctx.ply, ctx.activeColor, runtime, ctx.previousMove);

                if (isQuietMove) {
                    const int colorIndex = chess::Board::colorToIndex(ctx.activeColor);
                    const int malus = -((ctx.depth + 1) * (ctx.depth + 1));
                    constexpr int32_t MAX_HISTORY = 16384;
                    for (int i = 0; i < numSearchedQuiets - 1; ++i) {
                        int16_t& h = runtime.history[colorIndex][searchedQuiets[i].from][searchedQuiets[i].to];
                        int32_t hScore = h;
                        hScore += malus - hScore * std::abs(malus) / MAX_HISTORY;
                        h = clampHeuristic16(hScore);
                    }
                }
            }
            break;
        }
    }

    // Macro-step 4: Return best-score package with interruption-safe fallback.
    if (!searchedAnyMove && isInterrupted(runtime)) {
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
    bool allowNullMove) noexcept {

    // Macro-step 1: Node accounting and early terminal condition checks.
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &runtime.nodesSearched;
    ++(*counter);

    int32_t earlyScore = 0;
    if (checkEarlyTerminalConditions(b, runtime, ply, earlyScore)) {
        return earlyScore;
    }

    if (depth <= 0) {
        return quiescenceSearch(b, runtime, alpha, beta, ply, useTT, counter, allowTTWrite);
    }

    // Macro-step 2: Terminal/repetition/mate-distance pruning and TT prelude.
    const bool isPVNode = (static_cast<int64_t>(beta) - static_cast<int64_t>(alpha) > 1);

    if (ply > 0) {
        const int32_t matingAlpha = NEG_INF + ply;
        const int32_t matingBeta  = POS_INF - ply;
        if (alpha < matingAlpha) alpha = matingAlpha;
        if (beta > matingBeta)   beta = matingBeta;
        if (alpha >= beta) return alpha;
    }

    if (ply > 0) {
        // Threefold repetition is claimable draw: treat as terminal.
        if (b.isThreefoldRepetition()) {
            return repetitionDrawScore(b);
        }

        // Twofold is only a potential draw. Avoid premature draw cutoff when
        // the side to move is materially ahead and should keep pressing.
        if (b.isTwofoldRepetition()) {
            const int32_t matDelta = Evaluator::getMaterialDelta(b);
            constexpr int32_t TWOFOLD_AVOID_DRAW_MATERIAL_MARGIN = 220;
            const bool whiteToMove = (b.getActiveColor() == chess::Board::WHITE);
            const bool sideToMoveAhead = whiteToMove
                ? (matDelta > TWOFOLD_AVOID_DRAW_MATERIAL_MARGIN)
                : (matDelta < -TWOFOLD_AVOID_DRAW_MATERIAL_MARGIN);
            if (!sideToMoveAhead) {
                return repetitionDrawScore(b);
            }
        }
    }

    if (b.isFiftyMoveRule()) [[unlikely]] return 0;

    const uint64_t heavyMaterial =
        b.pawns_bb[0] | b.pawns_bb[1] |
        b.rooks_bb[0] | b.rooks_bb[1] |
        b.queens_bb[0] | b.queens_bb[1];
    if (heavyMaterial == 0ULL && hasInsufficientMaterialDraw(b)) [[unlikely]] {
        return 0;
    }

    const uint64_t hashKey = b.getHash();
    AlphaBeta bounds{alpha, beta};
    int32_t score = 0;
    const bool canUseTT = useTT && (runtime.transpositionTable != nullptr);
    if (canUseTT && handleSearchPrelude(runtime, depth, bounds, score, hashKey)) {
        return score;
    }

    // Macro-step 3: Build node state and apply null-move / reverse-futility pruning.
    SearchNodeState node{};
    node.activeColor = b.getActiveColor();
    node.usIsWhite = (node.activeColor == chess::Board::WHITE);
    node.inCheck = b.inCheck(node.activeColor);
    node.isPVNode = isPVNode;
    const int nonPawnMajorsAll = __builtin_popcountll(
        b.knights_bb[0] | b.knights_bb[1] |
        b.bishops_bb[0] | b.bishops_bb[1] |
        b.rooks_bb[0]   | b.rooks_bb[1]   |
        b.queens_bb[0]  | b.queens_bb[1]);
    node.isPawnEndgameForPruning =
        ((b.pawns_bb[0] | b.pawns_bb[1]) != 0ULL) && (nonPawnMajorsAll <= 4);

    node.staticEval = (ply > 0 && !node.inCheck) ? Evaluator::evaluate(b) : 0;

    const int side = chess::Board::colorToIndex(node.activeColor);
    const int nonPawnMajors = __builtin_popcountll(
        b.knights_bb[side] | b.bishops_bb[side] |
        b.rooks_bb[side]   | b.queens_bb[side]);
    const int32_t nmpEvalGate = node.usIsWhite
        ? (node.staticEval + 100)
        : (node.staticEval - 100);
    const bool canNullMove = allowNullMove
        && !node.isPVNode
        && !node.inCheck
        && ply > 0
        && depth >= 6
        && nonPawnMajors >= 3
        && isBetaCutoff(nmpEvalGate, alpha, beta, node.usIsWhite);

    if (canNullMove
        && tryNullMovePruning(b, node, runtime, depth, alpha, beta, ply,
                              canUseTT, allowTTWrite, allowHeuristicUpdates,
                              counter, score)) {
        return score;
    }

    const bool canReverseFutilityPrune =
        !node.isPVNode && !node.inCheck && !node.isPawnEndgameForPruning && ply > 0 && depth <= 3;
    if (canReverseFutilityPrune
        && tryReverseFutilityPruning(b, node, depth, alpha, beta, ply, score)) {
        return score;
    }

    // Macro-step 4: Generate/sort moves, recurse through searchMoves, and write TT.
    SearchContext ctx{
        depth, bounds.alpha, bounds.beta, ply, node.activeColor,
        previousMove, node.staticEval, node.inCheck, node.isPVNode, counter
    };

    const bool nodeInDoubleCheck = node.inCheck && b.isDoubleCheck(node.activeColor);
    MoveList<chess::Board::Move> moves = engine::MoveGenerator::generateLegalMoves(
        b, true, node.inCheck, nodeInDoubleCheck);
    if (moves.is_empty()) {
        return node.inCheck
            ? (node.usIsWhite ? (NEG_INF + ply) : (POS_INF - ply))
            : stalemateScoreFromMaterialDelta(Evaluator::getMaterialDelta(b));
    }

    static constexpr int INCREMENTAL_PICKER_BRANCHING_THRESHOLD = 40;
    const bool useIncrementalPicker = (moves.size >= INCREMENTAL_PICKER_BRANCHING_THRESHOLD);

    Sorter::MovePickerData movePicker;
    MoveList<chess::Board::Move> orderedMoves;
    int32_t* moveScores = nullptr;
    bool hasHashMove = false;

    if (useIncrementalPicker) {
        movePicker = Sorter::prepareMovePicker(
            moves,
            ply,
            b,
            node.usIsWhite,
            hashKey,
            runtime.history,
            runtime.killerMoves,
            runtime.counterMoves,
            runtime.captureHistory,
            canUseTT ? runtime.transpositionTable : nullptr,
            ctx.previousMove,
            runtime.orderingPenaltySamePawnOpening);
        hasHashMove = movePicker.hashMoveIsLegal;
        moveScores = movePicker.scores;
    } else {
        orderedMoves = Sorter::sortLegalMoves(
            moves,
            ply,
            b,
            node.usIsWhite,
            hashKey,
            runtime.history,
            runtime.killerMoves,
            runtime.counterMoves,
            runtime.captureHistory,
            canUseTT ? runtime.transpositionTable : nullptr,
            ctx.previousMove,
            &hasHashMove,
            runtime.orderingPenaltySamePawnOpening);
    }

    if (!hasHashMove && depth >= 6 && ply > 0) {
        ctx.depth -= 1;
    }

    const int32_t alphaOrig = bounds.alpha;
    const int32_t betaOrig = bounds.beta;

    MoveList<chess::Board::Move>& searchMovesList = useIncrementalPicker ? movePicker.moves : orderedMoves;
    SearchMoveResult result = searchMoves(
        b, searchMovesList, moveScores, node.usIsWhite, ctx, bounds, runtime, canUseTT, allowHeuristicUpdates, allowTTWrite);
    const int32_t best = result.score;

    if (isInterrupted(runtime)) {
        return Evaluator::evaluate(b);
    }

    if (canUseTT && allowTTWrite) {
        const auto flag = determineFlag(best, alphaOrig, betaOrig);
        const uint16_t encodedMove = TranspositionTable::Entry::encodeMove(
            result.move.from.index, result.move.to.index, result.move.promotionPiece);

        runtime.transpositionTable->store(
            hashKey,
            static_cast<uint8_t>(ctx.depth),
            clampToInt32(best),
            static_cast<uint8_t>(flag),
            encodedMove);
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
    // Macro-step 1: Node accounting and early terminal condition checks.
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &runtime.nodesSearched;
    ++(*counter);

    int32_t earlyScore = 0;
    if (checkEarlyTerminalConditions(b, runtime, ply, earlyScore)) {
        return earlyScore;
    }

    const bool canUseTT = useTT && (runtime.transpositionTable != nullptr);
    if (canUseTT) {
        const uint64_t hashKey = b.getHash();
        int32_t ttScore = 0;
        int32_t ttAlpha = 0;
        int32_t ttBeta = 0;
        toTTProbeBounds(alpha, beta, ttAlpha, ttBeta);
        if (runtime.transpositionTable->probe(hashKey, 0, ttAlpha, ttBeta, ttScore)) {
            return ttScore;
        }
    }

    const uint8_t activeColor = b.getActiveColor();
    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    const bool inCheck = b.inCheck(activeColor);

    static constexpr uint8_t MAX_QSEARCH_DEPTH = 48;
    if (ply >= MAX_QSEARCH_DEPTH) {
        if (inCheck) {
            const bool inDoubleCheck = b.isDoubleCheck(activeColor);
            MoveList<chess::Board::Move> evasions = engine::MoveGenerator::generateLegalMoves(
                b, true, true, inDoubleCheck);
            if (evasions.is_empty()) {
                return usIsWhite ? (NEG_INF + ply) : (POS_INF - ply);
            }
        }
        return Evaluator::evaluate(b);
    }

    // Macro-step 2: Handle in-check evasions as mandatory tactical recursion.
    if (inCheck) {
        MoveList<chess::Board::Move> evasions = engine::MoveGenerator::generateQSearchEvasions(b);
        if (evasions.is_empty()) {
            return usIsWhite ? (NEG_INF + ply) : (POS_INF - ply);
        }

        int32_t best = initialBest(usIsWhite);
        for (const auto& m : evasions) {
            if (shouldAbortSearch(runtime)) {
                markInterrupted(runtime);
                return Evaluator::evaluate(b);
            }

            chess::Board::MoveState state;
            doMoveWithPromotion(b, m, state);
            const int32_t score = quiescenceSearch(b, runtime, alpha, beta, ply + 1, canUseTT, counter, allowTTWrite);
            b.undoMove(m, state);

            if (isBetter(score, best, usIsWhite)) {
                best = score;
            }

            updateBound(score, alpha, beta, usIsWhite);
            if (isBetaCutoff(score, alpha, beta, usIsWhite)) {
                return cutoffValue(alpha, beta, usIsWhite);
            }
        }

        return best;
    }

    // Macro-step 3: Evaluate stand-pat and apply dynamic delta pruning guards.
    const int32_t standPat = Evaluator::evaluate(b);

    if (isBetaCutoff(standPat, alpha, beta, usIsWhite)) {
        return cutoffValue(alpha, beta, usIsWhite);
    }

    updateBound(standPat, alpha, beta, usIsWhite);

    //FIXME: Spostare costante fuori
    static constexpr int32_t EARLY_DELTA_MARGIN = 950;
    if (shouldDeltaPrune(standPat, EARLY_DELTA_MARGIN, alpha, beta, usIsWhite)) {
        return usIsWhite ? alpha : beta;
    }

    //FIXME: Chiamare namespace
    int32_t deltaMargin = QUEEN_VALUE;
    const int side = chess::Board::colorToIndex(activeColor);
    const uint64_t ourPawns = b.pawns_bb[side];
    //FIXME: Eliminare costanti magiche
    const uint64_t nearPromoPawns = usIsWhite
        ? (ourPawns & 0x00FF000000000000ULL)
        : (ourPawns & 0x000000000000FF00ULL);
    if (nearPromoPawns) {
        deltaMargin += 150;
    }

    const int32_t materialBalance = usIsWhite
        ? standPat
        : (standPat == NEG_INF ? POS_INF : -standPat);
    //FIXME: Eliminare costanti magiche
    if (materialBalance < -400) {
        deltaMargin += 150;
    } else if (materialBalance < -200) {
        deltaMargin += 75;
    }

    //FIXME: Eliminare costanti magiche
    const int runtimeDepth = runtime.depth;
    const int qsearchDepth = std::max(0, ply - runtimeDepth);
    if (qsearchDepth > 5) {
        deltaMargin -= 50 * ((qsearchDepth - 5) / 5);
        deltaMargin = std::max(deltaMargin, QUEEN_VALUE);
    }

    if (shouldDeltaPrune(standPat, deltaMargin, alpha, beta, usIsWhite)) {
        return usIsWhite ? alpha : beta;
    }

    // Macro-step 4: Generate tactical set, recurse, apply cutoffs, and optionally store TT.
    MoveList<chess::Board::Move> tacticalMoves = engine::MoveGenerator::generateQSearchTacticalMoves(
        b, standPat, alpha, beta, ply, usIsWhite, runtime.depth);
    if (tacticalMoves.is_empty()) {
        return standPat;
    }

    const int32_t alphaOrig = alpha;
    const int32_t betaOrig = beta;
    int32_t best = standPat;

    //FIXME: Eliminare costanti magiche
    for (const auto& m : tacticalMoves) {
        if (shouldAbortSearch(runtime)) {
            markInterrupted(runtime);
            return Evaluator::evaluate(b);
        }

        chess::Board::MoveState state;
        doMoveWithPromotion(b, m, state);
        const int32_t score = quiescenceSearch(b, runtime, alpha, beta, ply + 1, canUseTT, counter, allowTTWrite);
        b.undoMove(m, state);

        if (isBetter(score, best, usIsWhite)) {
            best = score;
        }

        updateBound(score, alpha, beta, usIsWhite);
        if (isBetaCutoff(score, alpha, beta, usIsWhite)) {
            if (canUseTT && allowTTWrite) {
                const uint64_t hashKey = b.getHash();
                const auto flag = determineFlag(best, alphaOrig, betaOrig);
                runtime.transpositionTable->store(
                    hashKey,
                    0,
                    clampToInt32(cutoffValue(alpha, beta, usIsWhite)),
                    static_cast<uint8_t>(flag));
            }
            return cutoffValue(alpha, beta, usIsWhite);
        }
    }

    if (canUseTT && allowTTWrite) {
        const uint64_t hashKey = b.getHash();
        const auto flag = determineFlag(best, alphaOrig, betaOrig);
        runtime.transpositionTable->store(hashKey, 0, clampToInt32(best), static_cast<uint8_t>(flag));
    }

    return best;
}

chess::Board::Move Searcher::getBestMove(
    chess::Board& rootBoard,
    const MoveList<chess::Board::Move>& moves,
    bool usIsWhite,
    SearchRuntime& runtime,
    int32_t alpha,
    int32_t beta) noexcept {
    // Macro-step 1: Initialize root state, order root moves, and decide YBWC mode.
    int32_t bestScore = initialBest(usIsWhite);
    chess::Board::Move bestMove = moves[0];
    constexpr int currPly = 1;
    uint64_t localNodes = 0;
    bool searchedAnyMove = false;

    MoveList<chess::Board::Move> orderedRootMoves = Sorter::sortLegalMoves(
        moves,
        0,
        rootBoard,
        usIsWhite,
        rootBoard.getHash(),
        runtime.history,
        runtime.killerMoves,
        runtime.counterMoves,
        runtime.captureHistory,
        runtime.transpositionTable,
        nullptr,
        nullptr,
        runtime.orderingPenaltySamePawnOpening);
    const MoveList<chess::Board::Move>& rootMoves = orderedRootMoves;

    const bool useYBWC = (rootMoves.size >= 10
        && runtime.depth >= DEFAULT_DEPTH - 2);

    //FIXME: Eliminare costanti magiche
    // Macro-step 2: Sequential PVS root search when YBWC is not profitable.
    if (!useYBWC) {
        for (int i = 0; i < rootMoves.size; ++i) {
            if (shouldAbortSearch(runtime)) {
                markInterrupted(runtime);
                break;
            }

            const auto& m = rootMoves[i];
            int32_t score = 0;
	    //FIXME: Fare prima interazione fuori ciclo e poi il resto paretendo da i=1
            if (i == 0) {
                score = searchRootMoveScore(rootBoard, m, runtime, alpha, beta, currPly, true, true, true, &localNodes);
            } else {
                int32_t nullAlpha = 0;
                int32_t nullBeta = 0;
                rootNullWindow(usIsWhite, alpha, beta, nullAlpha, nullBeta);

                score = searchRootMoveScore(rootBoard, m, runtime, nullAlpha, nullBeta, currPly, true, true, true, &localNodes);
                const bool shouldResearch = shouldResearchPVS(score, alpha, beta, usIsWhite);
                if (shouldResearch) {
                    score = searchRootMoveScore(rootBoard, m, runtime, alpha, beta, currPly, true, true, true, &localNodes);
                }
            }

            searchedAnyMove = true;
            if (isInterrupted(runtime)) {
                break;
            }

            updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);
            if (isBetaCutoff(bestScore, alpha, beta, usIsWhite)) break;
        }

        runtime.nodesSearched += localNodes;
        if (searchedAnyMove) runtime.eval = bestScore;
        return bestMove;
    }
    //FIXME: Eliminare scopo anonimo

    // Macro-step 3: YBWC root search (first move serial, remaining moves task-parallel).
    {
        const auto& firstMove = rootMoves[0];
        const int32_t score = searchRootMoveScore(rootBoard, firstMove, runtime, alpha, beta, currPly, true, true, true, &localNodes);
        searchedAnyMove = true;
        updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, firstMove);
    }

    if (isInterrupted(runtime)) {
        runtime.nodesSearched += localNodes;
        if (searchedAnyMove) runtime.eval = bestScore;
        return bestMove;
    }

    if (rootMoves.size <= 1) [[unlikely]] {
        runtime.nodesSearched += localNodes;
        if (searchedAnyMove) runtime.eval = bestScore;
        return bestMove;
    }

    const int32_t sharedAlpha = alpha;
    const int32_t sharedBeta = beta;
    int32_t nullAlpha = 0;
    int32_t nullBeta = 0;
    rootNullWindow(usIsWhite, sharedAlpha, sharedBeta, nullAlpha, nullBeta);

    std::array<int32_t, MAX_MOVES> threadScores;
    threadScores.fill(initialBest(usIsWhite));
    std::array<uint64_t, MAX_MOVES> threadNodeCounts {};
    std::array<uint8_t, MAX_MOVES> threadNeedsResearch {};

    int candidateThreads = std::max(1, rootMoves.size - 1);
    const int threadsToUse = std::max(1, std::min(runtime.maxThreads, candidateThreads));

    //FIXME: Fare funzione helper
    if (threadsToUse <= 1) {
        for (int i = 1; i < rootMoves.size; ++i) {
            if (shouldAbortSearch(runtime)) {
                markInterrupted(runtime);
                break;
            }

            chess::Board threadBoard = rootBoard;
            const auto m = rootMoves[i];
            uint64_t workerNodes = 0;
            const int32_t score = searchRootMoveScore(
                threadBoard, m, runtime, nullAlpha, nullBeta, currPly, true, false, false, &workerNodes);

            threadScores[i] = score;
            threadNodeCounts[i] = workerNodes;
            threadNeedsResearch[i] = shouldResearchPVS(score, sharedAlpha, sharedBeta, usIsWhite);
            if (isInterrupted(runtime)) {
                break;
            }
        }
    } else {
        const int totalJobs = rootMoves.size - 1;
        int estimatedChunk = std::max(1, totalJobs / (threadsToUse * 4));
        const int chunk = std::min(16, estimatedChunk);

	//FIXME: Eliminare indentazioni
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
                            if (!shouldAbortSearch(runtime)) {
                                chess::Board threadBoard = rootBoard;
                                for (int i = start; i < end; ++i) {
                                    if (shouldAbortSearch(runtime)) {
                                        break;
                                    }

                                    const auto m = rootMoves[i];
                                    uint64_t workerNodes = 0;
                                    const int32_t score = searchRootMoveScore(
                                        threadBoard, m, runtime, nullAlpha, nullBeta, currPly, true, false, false, &workerNodes);

                                    threadScores[i] = score;
                                    threadNodeCounts[i] = workerNodes;
                                    threadNeedsResearch[i] = shouldResearchPVS(score, sharedAlpha, sharedBeta, usIsWhite);
                                    if (isInterrupted(runtime)) {
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

    // Macro-step 4: Deterministic merge of parallel root results and final counters.
    for (int i = 1; i < rootMoves.size; ++i) {
        if (isInterrupted(runtime)) {
            break;
        }
        if (threadNodeCounts[i] == 0) continue;

        const auto& m = rootMoves[i];
        int32_t score = threadScores[i];
        if (threadNeedsResearch[i] != 0U) {
            uint64_t researchNodes = 0;
            score = searchRootMoveScore(rootBoard, m, runtime, alpha, beta, currPly, true, true, true, &researchNodes);
            localNodes += researchNodes;
            if (isInterrupted(runtime)) {
                break;
            }
        }

        updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);
        searchedAnyMove = true;

        if (isBetaCutoff(bestScore, alpha, beta, usIsWhite)) {
            break;
        }
    }

    localNodes = std::accumulate(threadNodeCounts.begin() + 1, threadNodeCounts.end(), localNodes);
    runtime.nodesSearched += localNodes;
    if (searchedAnyMove) runtime.eval = bestScore;
    return bestMove;
}

void Searcher::storeRootHashMove(
    const chess::Board& rootBoard,
    const chess::Board::Move& move,
    uint64_t depth,
    int32_t score,
    SearchRuntime& runtime,
    uint8_t flag) noexcept {
    //FIXME: Mettere in pre codizioni
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
    runtime.transpositionTable->store(rootBoard.getHash(), depth, clampToInt32(score), flag, encodedMove);
}

Searcher::IterativeSearchResult Searcher::runIterativeDeepening(
    chess::Board& rootBoard,
    SearchRuntime& runtime,
    uint64_t startDepth,
    uint64_t targetDepth,
    bool allowStop) noexcept {
    // Macro-step 1: Initialize iterative-deepening bounds and root legal move set.
    (void)allowStop; // kept for API compatibility with Engine flow.

    IterativeSearchResult result;
    const uint64_t firstDepth = std::max<uint64_t>(1, startDepth);
    const uint64_t maxDepth = std::max<uint64_t>(firstDepth, targetDepth);
    result.startDepth = firstDepth;
    result.targetDepth = maxDepth;

    MoveList<chess::Board::Move> moves = engine::MoveGenerator::generateLegalMoves(rootBoard);
    if (moves.is_empty()) {
        const uint8_t toMove = rootBoard.getActiveColor();
        if (rootBoard.kings_bb[0] == 0) {
            result.bestScore = NEG_INF;
        } else if (rootBoard.kings_bb[1] == 0) {
            result.bestScore = POS_INF;
        } else if (rootBoard.isCheckmate(toMove)) {
            result.bestScore = (toMove == chess::Board::WHITE) ? NEG_INF : POS_INF;
        } else if (rootBoard.isDraw(toMove)) {
            result.bestScore = 0;
        } else {
            result.bestScore = Evaluator::evaluate(rootBoard);
        }
        runtime.eval = result.bestScore;
        return result;
    }

    result.hasLegalMoves = true;
    uint64_t interruptedDepth = 0;
    const bool searchBestMoveForWhite = (rootBoard.getActiveColor() == chess::Board::WHITE);
    chess::Board::Move bestMove = moves[0];
    int32_t prevPrevScore = 0;
    int32_t prevScore = 0;
    bool hasPrevScore = false;
    bool hasPrevPrevScore = false;
    constexpr int32_t MATE_SCORE_THRESHOLD = POS_INF - 2048;
    auto absScore = [](int32_t v) noexcept -> int32_t {
        if (v == NEG_INF) return POS_INF;
        return (v >= 0) ? v : -v;
    };

    //FIXME: Fare funzione helper
    // Macro-step 2: Iterate depth-by-depth with aspiration windows when stable.
    for (uint64_t currentDepth = firstDepth; currentDepth <= maxDepth; ++currentDepth) {
        if (shouldAbortSearch(runtime)) {
            interruptedDepth = currentDepth;
            break;
        }

        runtime.depth = currentDepth;

        if (result.completedAnyDepth) {
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i] == bestMove) {
                    chess::Board::Move::rotate(moves, i);
                    break;
                }
            }
        }

        clearInterrupted(runtime);
        bool iterationCompleted = true;
        int32_t iterationAlpha = NEG_INF;
        int32_t iterationBeta = POS_INF;
        chess::Board::Move candidateBestMove = moves[0];

        const bool canUseAspiration =
            hasPrevScore
            && hasPrevPrevScore
            && result.completedAnyDepth
            && currentDepth >= 5
            && absScore(prevScore) < MATE_SCORE_THRESHOLD
            && absScore(prevPrevScore) < MATE_SCORE_THRESHOLD;

        if (!canUseAspiration) {
            candidateBestMove = getBestMove(rootBoard, moves, searchBestMoveForWhite, runtime);
            if (isInterrupted(runtime)) {
                iterationCompleted = false;
            }
        } else {
            const int64_t scoreDiff64 = static_cast<int64_t>(prevScore) - static_cast<int64_t>(prevPrevScore);
            const int64_t scoreSwing64 = (scoreDiff64 >= 0) ? scoreDiff64 : -scoreDiff64;
            const int32_t scoreSwing = std::min<int64_t>(scoreSwing64, POS_INF);
            int32_t windowDelta = std::clamp<int32_t>(40 + (scoreSwing / 2), 60, 220);
            constexpr int32_t WINDOW_HARD_CAP = 1500;
            constexpr int MAX_ASP_RESEARCHES = 6;
            int aspirationResearches = 0;
            int32_t centerScore = prevScore;
            int32_t aspAlpha = saturatingSub32(centerScore, windowDelta);
            int32_t aspBeta = saturatingAdd32(centerScore, windowDelta);

            while (true) {
                iterationAlpha = aspAlpha;
                iterationBeta = aspBeta;
                candidateBestMove = getBestMove(rootBoard, moves, searchBestMoveForWhite, runtime, aspAlpha, aspBeta);
                if (isInterrupted(runtime)) {
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
                ++result.aspirationResearches;
                if (failLow) {
                    ++result.aspirationFailLow;
                    centerScore = std::min(centerScore, score);
                } else {
                    ++result.aspirationFailHigh;
                    centerScore = std::max(centerScore, score);
                }

                windowDelta = std::min<int32_t>(WINDOW_HARD_CAP, windowDelta * 2 + 20);
                if (aspirationResearches >= MAX_ASP_RESEARCHES || windowDelta >= WINDOW_HARD_CAP) {
                    iterationAlpha = NEG_INF;
                    iterationBeta = POS_INF;
                    candidateBestMove = getBestMove(rootBoard, moves, searchBestMoveForWhite, runtime);
                    if (isInterrupted(runtime)) {
                        iterationCompleted = false;
                    }
                    break;
                }

                if (failLow) {
                    aspAlpha = std::max<int32_t>(NEG_INF, saturatingSub32(centerScore, windowDelta));
                    aspBeta = std::min<int32_t>(POS_INF, saturatingAdd32(centerScore, std::max<int32_t>(40, windowDelta / 2)));
                } else {
                    aspAlpha = std::max<int32_t>(NEG_INF, saturatingSub32(centerScore, std::max<int32_t>(40, windowDelta / 2)));
                    aspBeta = std::min<int32_t>(POS_INF, saturatingAdd32(centerScore, windowDelta));
                }
            }
        }

        if (!iterationCompleted) {
            interruptedDepth = currentDepth;
            break;
        }

        if (hasPrevScore) {
            prevPrevScore = prevScore;
            hasPrevPrevScore = true;
        }

        bestMove = candidateBestMove;
        prevScore = runtime.eval;
        hasPrevScore = true;
        result.completedAnyDepth = true;
        ++result.completedIterations;
        result.completedDepth = currentDepth;
        if ((currentDepth & 1ULL) == 0ULL) {
            result.completedEvenDepth = currentDepth;
        }
        result.bestMove = bestMove;
        result.bestScore = runtime.eval;
        result.rootScoreBound = determineFlag(result.bestScore, iterationAlpha, iterationBeta);
        storeRootHashMove(rootBoard, bestMove, currentDepth, runtime.eval, runtime, static_cast<uint8_t>(result.rootScoreBound));
    }

    // Macro-step 3: Publish interruption metadata and return aggregated result.
    result.interruptedDepth = interruptedDepth;
    return result;
}

} // namespace engine
