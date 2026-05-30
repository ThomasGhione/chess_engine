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
    auto forwardFill = [](uint64_t pawns, const std::array<uint64_t, 64>& fills) -> uint64_t {
        uint64_t result = 0ULL;
        while (pawns) {
            result |= fills[std::countr_zero(pawns)];
            pawns &= pawns - 1;
        }
        return result;
    };

    auto ownPieces = [&](int side) -> uint64_t {
        return b.knights_bb[side] | b.bishops_bb[side] | b.rooks_bb[side]
             | b.queens_bb[side]  | b.kings_bb[side];
    };

    const int whiteSpace = std::popcount(forwardFill(whitePawns, WHITE_FORWARD_FILL) & WHITE_SPACE_MASK & ~ownPieces(0));
    const int blackSpace = std::popcount(forwardFill(blackPawns, BLACK_FORWARD_FILL) & BLACK_SPACE_MASK & ~ownPieces(1));

    return (whiteSpace - blackSpace) * engine::SPACE_BONUS;
}

} // namespace engine
