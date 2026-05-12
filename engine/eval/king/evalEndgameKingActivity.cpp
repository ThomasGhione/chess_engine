#include "../evaluator.hpp"

namespace engine {

namespace {
constexpr std::array<int8_t, 64> initMinCenterDist() noexcept {
    std::array<int8_t, 64> d{};
    constexpr int CENTER[4] = {27, 28, 35, 36};
    for (int sq = 0; sq < 64; ++sq) {
        const int r = sq >> 3, f = sq & 7;
        int best = 99;
        for (int c : CENTER) {
            const int cr = c >> 3, cf = c & 7;
            const int dist = (r > cr ? r - cr : cr - r) + (f > cf ? f - cf : cf - f);
            if (dist < best) best = dist;
        }
        d[sq] = static_cast<int8_t>(best);
    }
    return d;
}
static constexpr auto MIN_CENTER_DIST = initMinCenterDist();
} // namespace

template<int Side>
inline int32_t Evaluator::evalEndgameKingActivitySide(const chess::Board& b) noexcept {
    const uint64_t kbb = b.kings_bb[Side];
    if (!kbb) [[unlikely]] return 0;

    const int sq = __builtin_ctzll(kbb);
    constexpr int sign = (Side == 0) ? 1 : -1;
    return sign * (7 - MIN_CENTER_DIST[sq]) * 10;
}

int32_t Evaluator::evalEndgameKingActivity(const chess::Board& b) noexcept {
    return evalEndgameKingActivitySide<0>(b) + evalEndgameKingActivitySide<1>(b);
}

} // namespace engine
