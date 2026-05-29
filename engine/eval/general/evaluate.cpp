#include "../evaluator.hpp"

namespace engine {

namespace {

struct EvalCacheEntry {
    uint64_t key = std::numeric_limits<uint64_t>::max();
    int32_t score = 0;
    uint8_t valid = 0;
};

static constexpr size_t EVAL_CACHE_SIZE = 1u << 11; // 2048 entries (~32 KiB), tests/perf tuned.
static constexpr uint64_t EVAL_CACHE_MASK = EVAL_CACHE_SIZE - 1u;

} // namespace

int32_t Evaluator::evaluate(const chess::Board& board) noexcept {
    const uint8_t activeColor = board.getActiveColor();
    const bool whiteToMove = (activeColor == chess::Board::WHITE);
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0) [[unlikely]] {
        // Side-to-move relative: losing our king is -POS_INF, capturing the
        // opponent's is +POS_INF.
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

    const int32_t material = board.getIncrementalMaterialDelta();
    int32_t psqtMg = 0;
    int32_t psqtEg = 0;
    board.getIncrementalPsqtMgEg(psqtMg, psqtEg);

    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const uint64_t occ = board.getPiecesBitMap();

    int32_t result;
    if (phase.pawnOnlyEndgame) [[unlikely]] {
        // No minor/major pieces left: skip full attack-data computation.
        const int32_t evalBase = material + psqtEg;
        result = evaluatePawnOnlyEndgamePhase(board, evalBase, whitePawns, blackPawns);
    } else {
        AttackData attackData[2];
        computeAttackData(attackData, board, occ);
        result = evaluateUnifiedPhase(board, material, psqtMg, psqtEg,
                                      whitePawns, blackPawns, occ, attackData, phase.w1024);
    }

    // Opposite-color bishop draw scaling: only meaningful when the smooth
    // phase weight is firmly on the endgame side of the curve.
    if (phase.w1024 <= 128) {
        result = applyOppColorBishopScaling(board, result);
    }

    // Contract: evaluate() returns a SIDE-TO-MOVE relative score (negamax).
    // Terms are computed white-centric internally; negate once at the boundary
    // for Black. (Cache stores the STM-relative value; the zobrist key already
    // encodes side to move, so W/B-to-move map to different cache slots.)
    if (!whiteToMove) result = -result;

    cacheEntry.key = evalCacheKey;
    cacheEntry.score = result;
    cacheEntry.valid = 1;
    return result;
}

int32_t Evaluator::evaluateUnifiedPhase(const chess::Board& b, int32_t materialEval,
                                         int32_t psqtMg, int32_t psqtEg,
                                         uint64_t whitePawns, uint64_t blackPawns, uint64_t occ,
                                         const AttackData data[2], int32_t w1024) noexcept {
    // Each feature contributes to mg, eg, or both, mirroring where it was
    // active in the old discrete phase model. The smooth phase weight w1024
    // (1024 = opening/MG, 0 = endgame) blends the two accumulators at the end.
    int32_t mgAcc = materialEval + psqtMg;
    int32_t egAcc = materialEval + psqtEg;

    // Bishop pair: always-on.
    const int32_t bishopPair = evalBishopPairBonusCached(b);
    mgAcc += bishopPair;
    egAcc += bishopPair;

    // Always-on scalar features.
    const int32_t hanging  = evalHangingPieces(b, data);
    const int32_t mobility = evalMobility(data);
    const int32_t space    = evalSpaceAdvantage(b, whitePawns, blackPawns);
    mgAcc += hanging + mobility + space;
    egAcc += hanging + mobility + space;

    // Phase-aware pair-returning features (mg/eg split already computed).
    const PhaseValue threats    = evalThreatsPair(b, data, occ);
    const PhaseValue pawnStruct = evalPawnStructureCachedPair(b, whitePawns, blackPawns);
    const PhaseValue kingAct    = evalKingActivityPair(b);
    const PhaseValue initiative = evalInitiativePair(b);
    mgAcc += threats.mg + pawnStruct.mg + kingAct.mg + initiative.mg;
    egAcc += threats.eg + pawnStruct.eg + kingAct.eg + initiative.eg;

    // MG-side-only features (faded out toward the endgame).
    mgAcc += evalMinorPieceDevelopmentCached(b);
    mgAcc += evalEarlyQueenCached(b);
    mgAcc += evalCastlingBonusCached(b);
    mgAcc += evalCentralControlCached(b, whitePawns, blackPawns);
    mgAcc += evalPieceCoordinationCached(b);
    mgAcc += evalOutpostsCached(b);
    mgAcc += evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data);
    mgAcc += evalBlockedPawnByBishopsCached(b);
    mgAcc += evalPawnForks(b);
    mgAcc += evalBlockedCenterWithPieces(b, occ);
    mgAcc += evalKingMiddlegame(b, whitePawns, blackPawns, data);

    // Mid+endgame features (absent in pure opening, present from EMG onward).
    const int32_t trapped      = evalTrappedPieces(b, occ);
    const int32_t badBishop    = evalBadBishopCached(b, whitePawns, blackPawns);
    const int32_t rooks        = evalRooksCached(b, whitePawns, blackPawns);
    const int32_t weakSquares  = evalWeakSquaresCached(b, whitePawns, blackPawns);
    const int32_t bishopKnight = evalBishopVsKnightCached(b, whitePawns, blackPawns);
    mgAcc += trapped + badBishop + rooks + weakSquares + bishopKnight;
    egAcc += trapped + badBishop + rooks + weakSquares + bishopKnight;

    // EG-side-only features (faded in toward the endgame).
    egAcc += evalEndgameKingActivity(b);
    egAcc += evalMopUp(b);
    egAcc += evalPassedPawnKeySquares(b, whitePawns, blackPawns);
    egAcc += evalRuleOfSquare(b, whitePawns, blackPawns);
    egAcc += evalRookEndgamePressure(b);
    egAcc += evalQueenEndgamePressure(b);
    egAcc += evalDoubleRookEndgame(b);

    return PhaseValue{mgAcc, egAcc}.blend(w1024);
}

int32_t Evaluator::evaluatePawnOnlyEndgamePhase(const chess::Board& b, int32_t materialAndEgPsqt,
                                                  uint64_t whitePawns, uint64_t blackPawns) noexcept {
    AttackData pawnAttacks[2]{};
    pawnAttacks[0].allAttacks = collectPawnAttacks(whitePawns, 0);
    pawnAttacks[1].allAttacks = collectPawnAttacks(blackPawns, 1);

    int32_t eval = materialAndEgPsqt;
    eval += evalHangingPieces(b, pawnAttacks);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, true);
    eval += evalKingActivity(b, true);
    eval += evalEndgameKingActivity(b);
    eval += evalMopUp(b);
    eval += evalPassedPawnKeySquares(b, whitePawns, blackPawns);
    eval += evalRuleOfSquare(b, whitePawns, blackPawns);
    eval += evalInitiative(b, true);

    return eval;
}

} // namespace engine
