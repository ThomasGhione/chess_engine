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
constexpr uint64_t WHITE_SPACE_MASK = 0x000000FFFFFF0000ULL; // ranks 2,3,4
// Black advances toward rank 7, so black's space is the vertical mirror of
// white's: ranks 3,4,5. (Was incorrectly identical to WHITE_SPACE_MASK.)
constexpr uint64_t BLACK_SPACE_MASK = 0x0000FFFFFF000000ULL; // ranks 3,4,5

} // namespace

int32_t Evaluator::evalSpaceAdvantage(const chess::Board& b,
                                      uint64_t whitePawns,
                                      uint64_t blackPawns) noexcept {
    auto forwardFill = [](uint64_t pawns, const std::array<uint64_t, 64>& fills) -> uint64_t {
        uint64_t result = 0ULL;
        while (pawns) {
            result |= fills[__builtin_ctzll(pawns)];
            pawns &= pawns - 1;
        }
        return result;
    };

    auto ownPieces = [&](int side) -> uint64_t {
        return b.knights_bb[side] | b.bishops_bb[side] | b.rooks_bb[side]
             | b.queens_bb[side]  | b.kings_bb[side];
    };

    const int whiteSpace = __builtin_popcountll(forwardFill(whitePawns, WHITE_FORWARD_FILL) & WHITE_SPACE_MASK & ~ownPieces(0));
    const int blackSpace = __builtin_popcountll(forwardFill(blackPawns, BLACK_FORWARD_FILL) & BLACK_SPACE_MASK & ~ownPieces(1));

    return (whiteSpace - blackSpace) * engine::SPACE_BONUS;
}

} // namespace engine
