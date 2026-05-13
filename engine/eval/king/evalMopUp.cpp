#include "../evaluator.hpp"
#include <algorithm>

namespace engine {

// Mop-up evaluation: when we have a large material advantage, reward
// driving the enemy king to a corner and closing in with our own king.
// Corner distance table: 0 = corner, 6 = center.
namespace {
constexpr std::array<int8_t, 64> initCornerDist() noexcept {
    std::array<int8_t, 64> d{};
    for (int sq = 0; sq < 64; ++sq) {
        const int r = sq >> 3, f = sq & 7;
        d[sq] = static_cast<int8_t>(std::min({r, 7 - r, f, 7 - f}));
    }
    return d;
}
static constexpr auto CORNER_DIST = initCornerDist();
} // namespace

int32_t Evaluator::evalMopUp(const chess::Board& b) noexcept {
    const int32_t matDelta = b.getIncrementalMaterialDelta();

    // Only activate when one side is clearly winning materially.
    // Use a threshold roughly equal to a minor piece (300 cp).
    constexpr int32_t MOPUP_THRESHOLD = 300;
    if (matDelta == 0) return 0;

    const int winningSide = (matDelta > 0) ? 0 : 1;
    const int losingSide  = winningSide ^ 1;
    const int advantage   = std::abs(matDelta);
    if (advantage < MOPUP_THRESHOLD) return 0;

    const uint64_t ourKingBB   = b.kings_bb[winningSide];
    const uint64_t enemyKingBB = b.kings_bb[losingSide];
    if (!ourKingBB || !enemyKingBB) [[unlikely]] return 0;

    const int ourKingSq   = __builtin_ctzll(ourKingBB);
    const int enemyKingSq = __builtin_ctzll(enemyKingBB);

    // Reward enemy king near a corner (CORNER_DIST: 0 = corner, 3 = center-ish).
    // Max contribution: (3 - 0) * CORNER_SCALE = 3 * 20 = 60.
    constexpr int32_t CORNER_SCALE = 20;
    const int32_t cornerBonus = (3 - CORNER_DIST[enemyKingSq]) * CORNER_SCALE;

    // Reward our king closing in on the enemy king.
    // Manhattan distance ranges 1..14; closer = better.
    // Max contribution: (14 - 1) * PROXIMITY_SCALE = 13 * 10 = 130.
    constexpr int32_t PROXIMITY_SCALE = 10;
    const int kingDist = manhattan(ourKingSq, enemyKingSq);
    const int32_t proximityBonus = (14 - kingDist) * PROXIMITY_SCALE;

    // Scale the whole bonus by material advantage (capped at 900 = queen).
    // At advantage = MOPUP_THRESHOLD (300): scale = 300/900 ~ 0.33.
    // At advantage >= 900: scale = 1.0.
    constexpr int32_t SCALE_CAP = 900;
    const int32_t rawBonus = cornerBonus + proximityBonus;
    const int32_t scaledBonus = rawBonus * std::min(advantage, SCALE_CAP) / SCALE_CAP;

    const int sign = (winningSide == 0) ? 1 : -1;
    return sign * scaledBonus;
}

} // namespace engine
