#include "../evaluator.hpp"

namespace engine {

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
