#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalBishopVsKnight(const chess::Board& b,
                                          uint64_t whitePawns,
                                          uint64_t blackPawns) noexcept {
    const int whiteBishops = std::popcount(b.bishops_bb[0]);
    const int blackBishops = std::popcount(b.bishops_bb[1]);
    const int whiteKnights = std::popcount(b.knights_bb[0]);
    const int blackKnights = std::popcount(b.knights_bb[1]);

    const bool whiteBishopSide  = (whiteBishops > 0 && blackKnights > 0 && whiteKnights == 0);
    const bool blackBishopSide  = (blackBishops > 0 && whiteKnights > 0 && blackKnights == 0);
    if (!whiteBishopSide && !blackBishopSide) return {};

    const int totalPawns = std::popcount(whitePawns | blackPawns);

    constexpr int NEUTRAL_PAWNS = 8;
    constexpr int32_t SCALE = 4;
    const int openness = NEUTRAL_PAWNS - totalPawns;

    int32_t score = 0;
    if (whiteBishopSide) score += openness * SCALE;
    if (blackBishopSide) score -= openness * SCALE;

    return PhaseValue{score, score};
}

} // namespace engine
