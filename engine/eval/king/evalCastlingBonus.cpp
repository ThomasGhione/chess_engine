#include "../evaluator.hpp"

namespace engine {

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
