#include "../evaluator.hpp"

namespace engine {

inline int32_t Evaluator::evalCastlingBonusSide(const chess::Board& b, int side) noexcept {
    const bool castleKs = b.getCastle(side == 0 ? 0 : 2);
    const bool castleQs = b.getCastle(side == 0 ? 1 : 3);
    const bool canCastle = castleKs || castleQs;
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return 0;

    const int kingFile = chess::Board::file(__builtin_ctzll(kingBB));
    const bool kingOnWing = kingFile <= 2 || kingFile >= 5;

    const int sign = (side == 0) ? 1 : -1;
    if (!canCastle && !kingOnWing) {
        return sign * (-engine::LOSS_OF_CASTLING_PENALTY);
    }

    return 0;
}

int32_t Evaluator::evalCastlingBonus(const chess::Board& b) noexcept {
    return evalCastlingBonusSide(b, 0) + evalCastlingBonusSide(b, 1);
}

} // namespace engine
