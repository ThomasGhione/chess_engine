#include "searcher.hpp"
#include "../movelist.hpp"
#include "../eval/evaluator.hpp"
#include "../movegen/movegen.hpp"

namespace engine {

// ============================================================================
// Main Alpha-Beta Search
// ============================================================================

int32_t Searcher::search(chess::Board& b, int32_t depth, int32_t alpha, int32_t beta, int ply,
                         const chess::Board::Move* previousMove, uint64_t* nodeCounter, bool allowNullMove) noexcept {
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &state_.nodesSearched;
    ++(*counter);

    if (shouldAbort()) {
        state_.interrupted.store(true, std::memory_order_relaxed);
        return Evaluator::evaluate(b);
    }

    // avoid stack overflow and out-of-bounds access on killerMoves/history
    if (ply >= static_cast<int>(SearchState::MAX_PLY) - 1) {
        return Evaluator::evaluate(b);
    }

    if (depth <= 0) {
        return quiesce(b, alpha, beta, ply, counter);
    }

    // PV node detection (full window vs null window), deterministic by construction.
    const bool isPVNode = (static_cast<int64_t>(beta) - static_cast<int64_t>(alpha) > 1);

    // =========================================================================
    // MATE DISTANCE PRUNING
    // =========================================================================
    // If we already found a mate shorter than what this node could possibly produce,
    // prune immediately. This significantly speeds up mate searches.
    if (ply > 0) {
        // Best possible score for side to move: mate in (ply+1) moves
        // Worst possible score: getting mated in ply moves
        const int32_t matingAlpha = NEG_INF + ply;
        const int32_t matingBeta  = POS_INF - ply;
        if (alpha < matingAlpha) alpha = matingAlpha;
        if (beta > matingBeta)   beta = matingBeta;
        if (alpha >= beta) return alpha;
    }

    if (ply > 0 && b.isTwofoldRepetition()) {
        return repetitionDrawScore(b);
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
    AlphaBeta bounds{alpha, beta}; // Prepare search structures
    int32_t score = 0;

    // Handle terminal nodes, check extensions, and transposition table lookups
    // NOTE: TT lookups are handled differently in Searcher - simplified for now
    // (will need to implement handleSearchPrelude equivalent if using TT)

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

    const int nonPawnMajors = __builtin_popcountll(
        b.knights_bb[node.usIsWhite ? 0 : 1] | b.bishops_bb[node.usIsWhite ? 0 : 1] |
        b.rooks_bb[node.usIsWhite ? 0 : 1]   | b.queens_bb[node.usIsWhite ? 0 : 1]);
    const int32_t nmpEvalGate = node.usIsWhite
        ? (node.staticEval + 100)
        : (node.staticEval - 100);
    const bool canNullMove = allowNullMove
        && !node.isPVNode
        && !node.inCheck
        && ply > 0
        && depth >= 6
        && nonPawnMajors >= 3
        && Engine::isBetaCutoff(nmpEvalGate, alpha, beta, node.usIsWhite);
    if (canNullMove
        && tryNullMovePruning(b, node, depth, alpha, beta, ply, counter, score)) {
        return score;
    }

    const bool canReverseFutilityPrune =
        !node.isPVNode && !node.inCheck && !node.isPawnEndgameForPruning && ply > 0 && depth <= 3;
    if (canReverseFutilityPrune
        && tryReverseFutilityPruning(b, node, depth, alpha, beta, ply, score)) {
        return score;
    }

    SearchContext ctx{depth, alpha, beta, ply, node.activeColor,
                      previousMove, node.staticEval, node.inCheck, node.isPVNode, counter};
    ChessMoveList moves = MoveGenerator::generateLegalMoves(b);
    if (moves.is_empty()) {
        return node.inCheck
            ? (node.usIsWhite ? (NEG_INF + ply) : (POS_INF - ply))
            : stalemateScoreFromMaterialDelta(Evaluator::getMaterialDelta(b));
    }

    const bool hasHashMove = sortLegalMoves(
        moves, ply, b, node.usIsWhite, hashKey, ctx.previousMove);

    if (!hasHashMove && depth >= 6 && ply > 0) {
        ctx.depth -= 1;
    }

    const int32_t alphaOrig = bounds.alpha;
    const int32_t betaOrig = bounds.beta;

    SearchMoveResult result = searchMoves(b, moves, node.usIsWhite, ctx, bounds);
    int32_t best = result.score;

    if (state_.interrupted.load(std::memory_order_relaxed)) {
        return Evaluator::evaluate(b);
    }

    if (config_.useTT) {
        const auto flag = tt::determineFlag(best, alphaOrig, betaOrig);
        
        const uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(
            result.move.from.index, result.move.to.index, result.move.promotionPiece);

        tt_.store(hashKey, static_cast<uint8_t>(ctx.depth), Engine::clampToTTScore(best),
                   static_cast<uint8_t>(flag), encodedMove);
    }
    return best;
}

} // namespace engine
