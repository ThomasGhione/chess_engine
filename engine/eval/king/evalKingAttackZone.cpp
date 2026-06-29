#include "../evaluator.hpp"

namespace engine {

inline void Evaluator::accumulateKingZoneAttackersAll(const chess::Board& b, int side, uint64_t kingZone, uint64_t occ,
                                                      uint64_t developedKnights, uint64_t developedBishops,
                                                      int& attackerCount, int32_t& attackWeight) noexcept {
    accumulateKingZoneAttackers<knightAttacksLookup>(
        developedKnights, kingZone, occ, engine::KING_ATTACK_WEIGHT_KNIGHT, attackerCount, attackWeight);
    accumulateKingZoneAttackers<pieces::getBishopAttacks>(
        developedBishops, kingZone, occ, engine::KING_ATTACK_WEIGHT_BISHOP, attackerCount, attackWeight);
    accumulateKingZoneAttackers<pieces::getRookAttacks>(
        b.rooks_bb[side], kingZone, occ, engine::KING_ATTACK_WEIGHT_ROOK, attackerCount, attackWeight);
    accumulateKingZoneAttackers<pieces::getQueenAttacks>(
        b.queens_bb[side], kingZone, occ, engine::KING_ATTACK_WEIGHT_QUEEN, attackerCount, attackWeight);
}

int32_t Evaluator::evalKingAttackZoneSide(const chess::Board& b, const AttackData data[2], int side, uint64_t occ, int32_t materialScale) noexcept {
    static constexpr int ATTACKER_SCALE_PERCENT[9] = {0, 0, 32, 52, 68, 80, 90, 97, 100};
    constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL; // rank 7 (White's 1st rank)
    constexpr uint64_t BLACK_MINOR_START = 0x00000000000000FFULL; // rank 0 (Black's 8th rank)

    const int oppSide = side ^ 1;
    const int sign = (side == 0) ? 1 : -1;

    const uint64_t enemyKingBB = b.kings_bb[oppSide];
    if (!enemyKingBB) [[unlikely]] return 0;

    const int enemyKingSq = std::countr_zero(enemyKingBB);
    const uint64_t kingZone = pieces::KING_ATTACKS[enemyKingSq] | chess::Board::bitMask(enemyKingSq);

    const uint64_t developedKnights = (side == 0)
        ? (b.knights_bb[side] & ~WHITE_MINOR_START)
        : (b.knights_bb[side] & ~BLACK_MINOR_START);
    const uint64_t developedBishops = (side == 0)
        ? (b.bishops_bb[side] & ~WHITE_MINOR_START)
        : (b.bishops_bb[side] & ~BLACK_MINOR_START);

    int attackerCount = 0;
    int32_t attackWeight = 0;

    accumulateKingZoneAttackersAll(b, side, kingZone, occ, developedKnights, developedBishops, attackerCount, attackWeight);
    if (attackerCount < 2) return 0;

    const uint64_t defenderMap = data[oppSide].allAttacks | pieces::KING_ATTACKS[enemyKingSq];
    const uint64_t zoneAttacks = data[side].allAttacks & kingZone;
    const int safeContacts = std::popcount(zoneAttacks & ~defenderMap);
    const int forcingContacts = std::popcount(zoneAttacks & defenderMap);

    int32_t attackUnits = attackWeight
        + safeContacts * engine::KING_SAFE_CONTACT_BONUS
        + forcingContacts * engine::KING_FORCING_CONTACT_BONUS;

    addAllKingCheckUnits(b, side, enemyKingSq, defenderMap, occ, attackUnits);

    const int scaleIndex = std::min(attackerCount, 8);
    int32_t attackDanger = (attackUnits * ATTACKER_SCALE_PERCENT[scaleIndex]) / 100;
    attackDanger = (attackDanger * materialScale) / 100;
    attackDanger = std::min<int32_t>(attackDanger, engine::KING_ATTACK_DANGER_CAP);

    return sign * attackDanger;
}

inline void Evaluator::addAllKingCheckUnits(const chess::Board& b, int side, int enemyKingSq, uint64_t defenderMap, uint64_t occ, int32_t& attackUnits) noexcept {
    const uint64_t knightChecks = b.knights_bb[side] & pieces::KNIGHT_ATTACKS[enemyKingSq];
    const uint64_t bishopChecks = b.bishops_bb[side] & pieces::getBishopAttacks(enemyKingSq, occ);
    const uint64_t rookChecks = b.rooks_bb[side] & pieces::getRookAttacks(enemyKingSq, occ);
    const uint64_t queenChecks = b.queens_bb[side] & pieces::getQueenAttacks(enemyKingSq, occ);

    addKingCheckUnits(knightChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS, engine::KING_FORCING_CHECK_BONUS, attackUnits);
    addKingCheckUnits(bishopChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS, engine::KING_FORCING_CHECK_BONUS, attackUnits);
    addKingCheckUnits(rookChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS + 4, engine::KING_FORCING_CHECK_BONUS + 2, attackUnits);
    addKingCheckUnits(queenChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS + 8, engine::KING_FORCING_CHECK_BONUS + 4, attackUnits);
}

} // namespace engine
