#include "evaluator.hpp"
namespace engine {

template<int Side>
inline int64_t Evaluator::evalEndgameKingActivitySide(const chess::Board& b) noexcept {
    static constexpr int CENTER[4] = {27, 28, 35, 36}; // d4 e4 d5 e5
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

int64_t Evaluator::evalEndgameKingActivity(const chess::Board& b) noexcept {
    int64_t scoreWhite = evalEndgameKingActivitySide<0>(b);
    int64_t scoreBlack = evalEndgameKingActivitySide<1>(b);

    return scoreBlack + scoreWhite;
}

} // namespace engine
