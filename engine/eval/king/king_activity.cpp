#include "../evaluator.hpp"

namespace engine {

template<bool IsEndgame>
inline int32_t Evaluator::evalKingActivitySide(const chess::Board& b, int side) noexcept {
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return 0;

    const int sign = (side == 0) ? 1 : -1;
    const int ksq = __builtin_ctzll(kingBB);
    const uint64_t proximityMask = KING_PROXIMITY_MASKS[ksq];

    if constexpr (IsEndgame) {
        const uint64_t friends =
            b.pawns_bb[side]   |
            b.knights_bb[side] |
            b.bishops_bb[side] |
            b.rooks_bb[side]   |
            b.queens_bb[side];
        const int friendsNearKing = __builtin_popcountll(friends & proximityMask);
        return sign * friendsNearKing * engine::KING_ACTIVITY_BONUS;
    }

    const int opp = side ^ 1;
    const uint64_t enemies =
        b.pawns_bb[opp]   |
        b.knights_bb[opp] |
        b.bishops_bb[opp] |
        b.rooks_bb[opp]   |
        b.queens_bb[opp];
    const int enemiesNearKing = __builtin_popcountll(enemies & proximityMask);
    return sign * enemiesNearKing * engine::KING_SAFETY_PENALTY;
}

int32_t Evaluator::evalKingActivity(const chess::Board& b, bool isEndgame) noexcept {
    return isEndgame
        ? (evalKingActivitySide<true>(b, 0) + evalKingActivitySide<true>(b, 1))
        : (evalKingActivitySide<false>(b, 0) + evalKingActivitySide<false>(b, 1));
}

template<int Side>
inline int32_t Evaluator::evalEndgameKingActivitySide(const chess::Board& b) noexcept {
    static constexpr int CENTER[4] = {27, 28, 35, 36};
    const uint64_t kbb = b.kings_bb[Side];
    if (!kbb) [[unlikely]] return 0;

    const int sign = (Side == 0) ? 1 : -1;
    const int sq = __builtin_ctzll(kbb);
    int best = manhattan(sq, CENTER[0]);
    best = std::min(best, manhattan(sq, CENTER[1]));
    best = std::min(best, manhattan(sq, CENTER[2]));
    best = std::min(best, manhattan(sq, CENTER[3]));
    return sign * (7 - best) * 10;
}

int32_t Evaluator::evalEndgameKingActivity(const chess::Board& b) noexcept {
    return evalEndgameKingActivitySide<0>(b) + evalEndgameKingActivitySide<1>(b);
}

} // namespace engine
