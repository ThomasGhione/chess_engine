#include "../evaluator.hpp"

namespace engine {

// Space advantage evaluation.
//
// Space = number of squares in the opponent's half of the board (ranks 3-6
// for white, ranks 2-5 for black) that are:
//   - behind the front-most own pawn on each file (forward fill), AND
//   - not occupied by own pieces (they're already "used").
//
// A larger space count means the side has more room to maneuver.
// The bonus is applied as the *difference* in space, so it's zero-sum.
// Scaled down in endgames (space matters less without pieces to maneuver).

namespace {
// Ranks 3-6 for white's space territory (ranks are 0-indexed from black's
// side: rank 0 = black's back rank, rank 7 = white's back rank).
// White advances toward rank 0, so white's "space" is ranks 2-5.
constexpr uint64_t WHITE_SPACE_MASK = 0x000000FFFFFF0000ULL; // ranks 2-5 (indices 2..5)
// Black advances toward rank 7, so black's "space" is ranks 2-5 as well
// (symmetric — both sides fight for the same central territory).
constexpr uint64_t BLACK_SPACE_MASK = 0x000000FFFFFF0000ULL;

} // namespace

int32_t Evaluator::evalSpaceAdvantage(const chess::Board& b,
                                      uint64_t whitePawns,
                                      uint64_t blackPawns,
                                      uint64_t occ) noexcept {
    // White space: forward-fill white pawns (toward rank 0), intersect with
    // space mask, exclude squares occupied by own pieces.
    uint64_t whiteFill = 0ULL;
    {
        uint64_t p = whitePawns;
        while (p) {
            const int sq = __builtin_ctzll(p);
            p &= p - 1;
            whiteFill |= WHITE_FORWARD_FILL[sq];
        }
    }
    const uint64_t whiteOwn = b.knights_bb[0] | b.bishops_bb[0] | b.rooks_bb[0]
                            | b.queens_bb[0]  | b.kings_bb[0];
    const int whiteSpace = __builtin_popcountll(whiteFill & WHITE_SPACE_MASK & ~whiteOwn);

    // Black space: forward-fill black pawns (toward rank 7).
    uint64_t blackFill = 0ULL;
    {
        uint64_t p = blackPawns;
        while (p) {
            const int sq = __builtin_ctzll(p);
            p &= p - 1;
            blackFill |= BLACK_FORWARD_FILL[sq];
        }
    }
    const uint64_t blackOwn = b.knights_bb[1] | b.bishops_bb[1] | b.rooks_bb[1]
                            | b.queens_bb[1]  | b.kings_bb[1];
    const int blackSpace = __builtin_popcountll(blackFill & BLACK_SPACE_MASK & ~blackOwn);

    (void)occ;
    return (whiteSpace - blackSpace) * engine::SPACE_BONUS;
}

} // namespace engine
