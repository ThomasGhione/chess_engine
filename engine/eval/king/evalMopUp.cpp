#include <bit>
#include "../evaluator.hpp"
#include <algorithm>

namespace engine {

// Mop-up evaluation: when we have a large material advantage, reward
// driving the enemy king to a corner and closing in with our own king.
// Corner distance table: 0 = corner, 6 = center.
namespace {
// Chebyshev distance to nearest corner (a1/a8/h1/h8). 0=corner, 6=centre.
constexpr std::array<int8_t, 64> initCornerDist() noexcept {
    std::array<int8_t, 64> d{};
    for (int sq = 0; sq < 64; ++sq) {
        const int r = sq >> 3, f = sq & 7;
        const int dr = std::min(r, 7 - r);
        const int df = std::min(f, 7 - f);
        d[sq] = static_cast<int8_t>(std::max(dr, df));
    }
    return d;
}
static constexpr auto CORNER_DIST = initCornerDist();
} // namespace

int32_t Evaluator::evalMopUp(const chess::Board& b) noexcept {
    const int32_t matDelta = b.getIncrementalMaterialDelta();

    constexpr int32_t MOPUP_THRESHOLD = 300;
    if (matDelta == 0) return 0;

    const int winningSide = (matDelta > 0) ? 0 : 1;
    const int losingSide  = winningSide ^ 1;
    const int advantage   = std::abs(matDelta);
    if (advantage < MOPUP_THRESHOLD) return 0;

    const uint64_t ourKingBB   = b.kings_bb[winningSide];
    const uint64_t enemyKingBB = b.kings_bb[losingSide];
    if (!ourKingBB || !enemyKingBB) [[unlikely]] return 0;

    const int ourKingSq   = std::countr_zero(ourKingBB);
    const int enemyKingSq = std::countr_zero(enemyKingBB);

    // Enemy king near corner: CORNER_DIST 0=corner, 6=centre. Max 6*40=240.
    constexpr int32_t CORNER_SCALE = 40;
    const int32_t cornerBonus = (6 - CORNER_DIST[enemyKingSq]) * CORNER_SCALE;

    // Our king closing in: Chebyshev distance 1..7. Max (7-1)*20=120.
    constexpr int32_t PROXIMITY_SCALE = 20;
    const int kingDist = chebyshev(ourKingSq, enemyKingSq);
    const int32_t proximityBonus = (7 - kingDist) * PROXIMITY_SCALE;

    const int32_t rawBonus = cornerBonus + proximityBonus;
    const int sign = (winningSide == 0) ? 1 : -1;
    return sign * rawBonus;
}

} // namespace engine
