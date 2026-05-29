#include <bit>
#include "../evaluator.hpp"

namespace engine {

template<bool IsEndgame>
inline int32_t Evaluator::evalKingActivitySide(const chess::Board& b, int side) noexcept {
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return 0;

    const int sign = (side == 0) ? 1 : -1;
    const int ksq = std::countr_zero(kingBB);
    const uint64_t proximityMask = KING_PROXIMITY_MASKS[ksq];

    if constexpr (IsEndgame) {
        const uint64_t friends =
            b.pawns_bb[side]   |
            b.knights_bb[side] |
            b.bishops_bb[side] |
            b.rooks_bb[side]   |
            b.queens_bb[side];
        const int friendsNearKing = std::popcount(friends & proximityMask);
        return sign * friendsNearKing * engine::KING_ACTIVITY_BONUS;
    }

    const int opp = side ^ 1;
    const uint64_t enemies =
        b.pawns_bb[opp]   |
        b.knights_bb[opp] |
        b.bishops_bb[opp] |
        b.rooks_bb[opp]   |
        b.queens_bb[opp];
    const int enemiesNearKing = std::popcount(enemies & proximityMask);
    return sign * enemiesNearKing * engine::KING_SAFETY_PENALTY;
}

int32_t Evaluator::evalKingActivity(const chess::Board& b, bool isEndgame) noexcept {
    return isEndgame
        ? (evalKingActivitySide<true>(b, 0) + evalKingActivitySide<true>(b, 1))
        : (evalKingActivitySide<false>(b, 0) + evalKingActivitySide<false>(b, 1));
}

} // namespace engine
