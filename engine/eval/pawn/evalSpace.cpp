#include <bit>
#include "../evaluator.hpp"

namespace engine {

namespace {
constexpr uint64_t WHITE_SPACE_MASK = 0x000000FFFFFF0000ULL; // ranks 2,3,4
constexpr uint64_t BLACK_SPACE_MASK = 0x0000FFFFFF000000ULL; // ranks 3,4,5
} // namespace

PhaseValue Evaluator::evalSpaceAdvantage(const chess::Board& b,
                                          uint64_t whitePawns,
                                          uint64_t blackPawns) noexcept {
    // FORWARD_FILL[sq] depends only on the rank, and it is monotonic in rank, so
    // the OR over a pawn set equals the forward fill of its rank-extreme pawn:
    // for White the highest-index pawn (msb), for Black the lowest (lsb). O(1),
    // bit-identical to the previous per-pawn OR loop.
    const uint64_t whiteFill = whitePawns ? WHITE_FORWARD_FILL[63 - std::countl_zero(whitePawns)] : 0ULL;
    const uint64_t blackFill = blackPawns ? BLACK_FORWARD_FILL[std::countr_zero(blackPawns)] : 0ULL;

    auto ownPieces = [&](int side) -> uint64_t {
        return b.knights_bb[side] | b.bishops_bb[side] | b.rooks_bb[side]
             | b.queens_bb[side]  | b.kings_bb[side];
    };

    const int whiteSpace = std::popcount(whiteFill & WHITE_SPACE_MASK & ~ownPieces(0));
    const int blackSpace = std::popcount(blackFill & BLACK_SPACE_MASK & ~ownPieces(1));

    return (whiteSpace - blackSpace) * engine::SPACE_BONUS;
}

} // namespace engine
