#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evaluateOpeningPhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopmentCached(b);
    eval += evalEarlyQueenCached(b);
    eval += evalCastlingBonusCached(b);
    eval += evalHangingPieces(b, data);
    eval += evalThreats(b, data, b.getPiecesBitMap(), false);
    eval += evalPawnForks(b);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalPieceCoordinationCached(b);
    eval += evalOutpostsCached(b);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalSpaceAdvantage(b, whitePawns, blackPawns, b.getPiecesBitMap());
    eval += evalMobility(data);
    eval += (evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data) * engine::KING_SAFETY_OPENING_SCALE_PERCENT) / 100;
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishops(b);

    return eval;
}

int32_t Evaluator::evaluateEarlyMiddlegamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopmentCached(b);
    eval += evalCastlingBonusCached(b);
    eval += evalHangingPieces(b, data);
    eval += evalThreats(b, data, occ, false);
    eval += evalPawnForks(b);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalSpaceAdvantage(b, whitePawns, blackPawns, occ);
    eval += evalMobility(data);
    eval += evalOutpostsCached(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishops(b);

    return eval;
}

int32_t Evaluator::evaluateMiddlegamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalHangingPieces(b, data);
    eval += evalThreats(b, data, occ, false);
    eval += evalPawnForks(b);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalBlockedCenterWithPieces(b, occ);
    eval += evalMobility(data);
    eval += evalPieceCoordinationCached(b);
    eval += evalOutpostsCached(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalWeakSquares(b, whitePawns, blackPawns);
    eval += evalBishopVsKnight(b, whitePawns, blackPawns);
    eval += evalSpaceAdvantage(b, whitePawns, blackPawns, occ);
    eval += evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data);
    eval += evalKingActivity(b, false);
    eval += evalCastlingBonusCached(b);
    eval += evalKingAttackZone(b, data);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishops(b);

    return eval;
}

int32_t Evaluator::evaluateEndgamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalHangingPieces(b, data);
    eval += evalThreats(b, data, occ, true);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, true);
    eval += evalKingActivity(b, true);
    eval += evalEndgameKingActivity(b);
    eval += evalMopUp(b);
    eval += evalPassedPawnKeySquares(b, whitePawns, blackPawns);
    eval += evalWeakSquares(b, whitePawns, blackPawns);
    eval += evalBishopVsKnight(b, whitePawns, blackPawns);
    eval += evalSpaceAdvantage(b, whitePawns, blackPawns, occ);
    eval += evalMobility(data);
    eval += evalTrappedPieces(b, occ);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalRookEndgamePressure(b);
    eval += evalQueenEndgamePressure(b);
    eval += evalDoubleRookEndgame(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, true);

    return eval;
}

int32_t Evaluator::evaluatePawnOnlyEndgamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    AttackData pawnAttacks[2]{};

    uint64_t pawns = whitePawns;
    while (pawns) {
        pawnAttacks[0].allAttacks |= pieces::PAWN_ATTACKS[0][popLSB(pawns)];
    }

    pawns = blackPawns;
    while (pawns) {
        pawnAttacks[1].allAttacks |= pieces::PAWN_ATTACKS[1][popLSB(pawns)];
    }

    eval += evalHangingPieces(b, pawnAttacks);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, true);
    eval += evalKingActivity(b, true);
    eval += evalEndgameKingActivity(b);
    eval += evalMopUp(b);
    eval += evalPassedPawnKeySquares(b, whitePawns, blackPawns);
    eval += evalInitiative(b, true);

    return eval;
}

} // namespace engine
