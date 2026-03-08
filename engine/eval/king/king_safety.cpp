#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalKingSafetyWithAttackData(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    const bool whiteCastleKs = b.getCastle(0);
    const bool whiteCastleQs = b.getCastle(1);
    const bool blackCastleKs = b.getCastle(2);
    const bool blackCastleQs = b.getCastle(3);

    return evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 0)
         + evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 1);
}

int32_t Evaluator::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const uint64_t occ = b.getPiecesBitMap();
    AttackData attackData[2];
    computeAttackData(attackData, b, occ);
    return evalKingSafetyWithAttackData(b, whitePawns, blackPawns, attackData);
}

inline int32_t Evaluator::evalCastlingBonusSide(const chess::Board& b, int side) noexcept {
    static constexpr uint64_t CASTLED_MASK[2] = {
        chess::Board::bitMask(62) | chess::Board::bitMask(58),
        chess::Board::bitMask(6)  | chess::Board::bitMask(2)
    };

    const bool castleKs = b.getCastle(side == 0 ? 0 : 2);
    const bool castleQs = b.getCastle(side == 0 ? 1 : 3);
    const bool rightsLost = !castleKs && !castleQs;
    const bool hasCastled = (b.kings_bb[side] & CASTLED_MASK[side]) && rightsLost;
    const bool canCastle = castleKs || castleQs;

    const int sign = (side == 0) ? 1 : -1;
    int32_t score = 0;
    if (hasCastled) score += sign * engine::CASTLING_BONUS;
    if (!hasCastled && !canCastle) score += sign * (-engine::LOSS_OF_CASTLING_PENALTY);
    return score;
}

int32_t Evaluator::evalCastlingBonus(const chess::Board& b) noexcept {
    return evalCastlingBonusSide(b, 0) + evalCastlingBonusSide(b, 1);
}

} // namespace engine
