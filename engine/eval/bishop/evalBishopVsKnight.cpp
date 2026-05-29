#include <bit>
#include "../evaluator.hpp"

namespace engine {

// Bishop vs knight preference based on pawn structure.
//
// Bishops prefer open positions (few pawns, open diagonals).
// Knights prefer closed positions (many pawns, blocked center).
//
// We give a bonus to the side with the bishop when the position is open,
// and a bonus to the side with the knight when the position is closed.
// Both are relative to the opponent's piece mix.

int32_t Evaluator::evalBishopVsKnight(const chess::Board& b,
                                      uint64_t whitePawns,
                                      uint64_t blackPawns) noexcept {
    const int whiteBishops = std::popcount(b.bishops_bb[0]);
    const int blackBishops = std::popcount(b.bishops_bb[1]);
    const int whiteKnights = std::popcount(b.knights_bb[0]);
    const int blackKnights = std::popcount(b.knights_bb[1]);

    // Only meaningful when the imbalance exists.
    const bool whiteBishopSide  = (whiteBishops > 0 && blackKnights > 0 && whiteKnights == 0);
    const bool blackBishopSide  = (blackBishops > 0 && whiteKnights > 0 && blackKnights == 0);
    if (!whiteBishopSide && !blackBishopSide) return 0;

    const int totalPawns = std::popcount(whitePawns | blackPawns);

    // Signed bonus: positive = bishop side benefits. At 8 pawns: neutral.
    // Below 8 pawns (open): bishop +. Above 8 (closed): knight +.
    constexpr int NEUTRAL_PAWNS = 8;
    constexpr int32_t SCALE = 4; // cp per pawn away from neutral
    // Fewer pawns => more open => favors the bishop side.
    const int openness = NEUTRAL_PAWNS - totalPawns; // positive = open (few pawns), negative = closed

    int32_t score = 0;
    if (whiteBishopSide) {
        // Positive openness → bishop better → white bonus.
        score += openness * SCALE;
    }
    if (blackBishopSide) {
        // Positive openness → bishop better → black bonus (subtract from white).
        score -= openness * SCALE;
    }

    return score;
}

} // namespace engine
