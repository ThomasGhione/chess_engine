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
    const int whiteBishops = __builtin_popcountll(b.bishops_bb[0]);
    const int blackBishops = __builtin_popcountll(b.bishops_bb[1]);
    const int whiteKnights = __builtin_popcountll(b.knights_bb[0]);
    const int blackKnights = __builtin_popcountll(b.knights_bb[1]);

    // Only meaningful when the imbalance exists.
    const bool whiteBishopSide  = (whiteBishops > 0 && blackKnights > 0 && whiteKnights == 0);
    const bool blackBishopSide  = (blackBishops > 0 && whiteKnights > 0 && blackKnights == 0);
    if (!whiteBishopSide && !blackBishopSide) return 0;

    const int totalPawns = __builtin_popcountll(whitePawns | blackPawns);

    // Openness: 0 (16 pawns, very closed) → 16 (0 pawns, fully open).
    // Bishop advantage peaks at ~4-8 pawns total (open middlegame/endgame).
    // Knight advantage peaks at 14-16 pawns (blocked center).
    // Map to a signed bonus: positive = bishop side benefits.
    // At 8 pawns: neutral. Above 8: bishop +. Below 8: knight +.
    constexpr int NEUTRAL_PAWNS = 8;
    constexpr int32_t SCALE = 4; // cp per pawn away from neutral
    const int openness = totalPawns - NEUTRAL_PAWNS; // negative = closed, positive = open

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
