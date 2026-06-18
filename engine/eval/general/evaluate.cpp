#include "../evaluator.hpp"

namespace engine {

namespace {

struct EvalCacheEntry {
    uint64_t key = std::numeric_limits<uint64_t>::max();
    int32_t score = 0;
    uint8_t valid = 0;
};

static constexpr size_t EVAL_CACHE_SIZE = 1u << 11;
static constexpr uint64_t EVAL_CACHE_MASK = EVAL_CACHE_SIZE - 1u;

} // namespace

int32_t Evaluator::evaluate(const chess::Board& board) noexcept {
    const uint8_t activeColor = board.getActiveColor();
    const bool whiteToMove = (activeColor == chess::Board::WHITE);
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0) [[unlikely]] {
        if (board.kings_bb[0] == 0) return whiteToMove ? -POS_INF : POS_INF;
        return whiteToMove ? POS_INF : -POS_INF;
    }

    thread_local std::array<EvalCacheEntry, EVAL_CACHE_SIZE> evalCache{};

    const uint64_t evalCacheKey = board.getHash();
    EvalCacheEntry& cacheEntry = evalCache[(evalCacheKey * 0xBF58476D1CE4E5B9ULL) & EVAL_CACHE_MASK];
    if (cacheEntry.valid && cacheEntry.key == evalCacheKey) [[likely]] {
        return cacheEntry.score;
    }

    const PhaseInfo phase = classifyPhase(board);

    const int32_t materialMg = board.getIncrementalMaterialMg();
    const int32_t materialEg = board.getIncrementalMaterialEg();
    int32_t psqtMg = 0;
    int32_t psqtEg = 0;
    board.getIncrementalPsqtMgEg(psqtMg, psqtEg);

    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const uint64_t occ = board.getPiecesBitMap();

    int32_t result;
    if (phase.pawnOnlyEndgame) [[unlikely]] {
        const int32_t evalBase = materialEg + psqtEg;
        result = evaluatePawnOnlyEndgamePhase(board, evalBase, whitePawns, blackPawns);
    } else {
        AttackData attackData[2];
        computeAttackData(attackData, board, occ);
        result = evaluateUnifiedPhase(board, materialMg, materialEg, psqtMg, psqtEg,
                                      whitePawns, blackPawns, occ, attackData, phase.w1024);
    }

    if (phase.w1024 <= 128) {
        result = applyOppColorBishopScaling(board, result);
    }

    if (!whiteToMove) result = -result;

    cacheEntry.key = evalCacheKey;
    cacheEntry.score = result;
    cacheEntry.valid = 1;
    return result;
}

int32_t Evaluator::evaluateUnifiedPhase(const chess::Board& b, int32_t materialMg, int32_t materialEg,
                                         int32_t psqtMg, int32_t psqtEg,
                                         uint64_t whitePawns, uint64_t blackPawns, uint64_t occ,
                                         const AttackData data[2], int32_t w1024) noexcept {
    PhaseValue acc{materialMg + psqtMg, materialEg + psqtEg};

    // Always-on terms.
    acc += evalBishopPairBonusCached(b);
    acc += evalHangingPieces(b, data);
    acc += evalMobility(data);
    acc += evalSpaceAdvantage(b, whitePawns, blackPawns);

    // Phase-aware features (mg/eg split baked in by the helper).
    acc += evalThreatsPair(b, data, occ);
    acc += evalPawnStructureCachedPair(b, whitePawns, blackPawns);
    acc += evalKingActivityPair(b);
    acc += evalInitiativePair(b);

    constexpr int32_t W_DROP_EG = 870;
    constexpr int32_t W_DROP_MG = 154;

    // MG-side-only features (faded out toward endgame).
    if (w1024 > W_DROP_MG) {
        acc += evalMinorPieceDevelopmentCached(b);
        acc += evalEarlyQueenCached(b);
        acc += evalCastlingBonusCached(b);
        acc += evalCentralControlCached(b, whitePawns, blackPawns);
        acc += evalPieceCoordinationCached(b);
        acc += evalOutpostsCached(b);
        acc += evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data);
        acc += evalBlockedPawnByBishops(b);
        acc += evalPawnForks(b);
        acc += evalBlockedCenterWithPieces(b, occ);
        acc += evalKingMiddlegame(b, whitePawns, blackPawns, data);
    }

    // Mid+endgame features (always computed when not in pure opening).
    acc += evalTrappedPieces(b, occ);
    acc += evalBadBishopCached(b, whitePawns, blackPawns);
    acc += evalRooksCached(b, whitePawns, blackPawns);
    acc += evalWeakSquaresCached(b, whitePawns, blackPawns);
    acc += evalBishopVsKnightCached(b, whitePawns, blackPawns);

    // EG-side-only features (faded in toward endgame).
    if (w1024 < W_DROP_EG) {
        acc += evalEndgameKingActivity(b);
        acc += evalMopUp(b);
        acc += evalPassedPawnKeySquares(b, whitePawns, blackPawns);
        acc += evalRuleOfSquare(b, whitePawns, blackPawns);
        acc += evalRookEndgamePressure(b);
        acc += evalQueenEndgamePressure(b);
        acc += evalDoubleRookEndgame(b);
    }

    return acc.blend(w1024);
}

int32_t Evaluator::evaluatePawnOnlyEndgamePhase(const chess::Board& b, int32_t materialAndEgPsqt,
                                                  uint64_t whitePawns, uint64_t blackPawns) noexcept {
    AttackData pawnAttacks[2]{};
    pawnAttacks[0].allAttacks = collectPawnAttacks(whitePawns, 0);
    pawnAttacks[1].allAttacks = collectPawnAttacks(blackPawns, 1);

    // Each sub-eval returns PhaseValue; we sink to .eg since this is the
    // pawn-only endgame fast path.
    PhaseValue acc{0, materialAndEgPsqt};
    acc += evalHangingPieces(b, pawnAttacks);
    acc += evalPawnStructureCached(b, whitePawns, blackPawns, true);
    acc += evalKingActivity(b, true);
    acc += evalEndgameKingActivity(b);
    acc += evalMopUp(b);
    acc += evalPassedPawnKeySquares(b, whitePawns, blackPawns);
    acc += evalRuleOfSquare(b, whitePawns, blackPawns);
    acc += evalInitiativePair(b);
    return acc.eg;
}

} // namespace engine
