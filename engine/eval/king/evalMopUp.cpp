#include "../evaluator.hpp"

namespace engine {

namespace {
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

PhaseValue Evaluator::evalMopUp(const chess::Board& b) noexcept {
    const int32_t matDelta = b.getIncrementalMaterialDelta();

    constexpr int32_t MOPUP_THRESHOLD = 300;
    if (matDelta == 0) return {};

    const int winningSide = (matDelta > 0) ? 0 : 1;
    const int losingSide  = winningSide ^ 1;
    const int advantage   = std::abs(matDelta);
    if (advantage < MOPUP_THRESHOLD) return {};

    const uint64_t ourKingBB   = b.kings_bb[winningSide];
    const uint64_t enemyKingBB = b.kings_bb[losingSide];
    if (!ourKingBB || !enemyKingBB) [[unlikely]] return {};

    const int ourKingSq   = std::countr_zero(ourKingBB);
    const int enemyKingSq = std::countr_zero(enemyKingBB);

    constexpr int32_t CORNER_SCALE    = 40;
    constexpr int32_t PROXIMITY_SCALE = 20;
    const int32_t cornerBonus    = (6 - CORNER_DIST[enemyKingSq]) * CORNER_SCALE;
    const int kingDist           = chebyshev(ourKingSq, enemyKingSq);
    const int32_t proximityBonus = (7 - kingDist) * PROXIMITY_SCALE;

    const int sign = (winningSide == 0) ? 1 : -1;
    // Pure endgame: mop-up is meaningless in the middlegame.
    return PhaseValue{0, sign * (cornerBonus + proximityBonus)};
}

} // namespace engine
