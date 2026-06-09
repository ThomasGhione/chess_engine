#include <bit>
#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalWeakSquares(const chess::Board& b,
                                       uint64_t whitePawns,
                                       uint64_t blackPawns) noexcept {
    constexpr uint64_t CENTER_EXTENDED = 0x00003C3C3C3C0000ULL;

    // fwd[sq] depends only on rank, so the per-pawn OR collapses onto the
    // rank-extreme pawn of each subset (shift/AND distribute over OR). The two
    // subsets exclude the edge files whose shift would wrap. Bit-identical to
    // the old per-pawn loop, O(1).
    auto buildHoles = [](uint64_t ownPawns,
                         const std::array<uint64_t, 64>& fwd, bool isWhite) -> uint64_t {
        const uint64_t nonFileA = ownPawns & ~FILE_MASKS[0]; // file > 0 (can shift >>1)
        const uint64_t nonFileH = ownPawns & ~FILE_MASKS[7]; // file < 7 (can shift <<1)
        auto extremeFwd = [&](uint64_t subset) -> uint64_t {
            if (!subset) return 0ULL;
            return fwd[isWhite ? (63 - std::countl_zero(subset)) : std::countr_zero(subset)];
        };
        const uint64_t attacks = ((extremeFwd(nonFileA) >> 1) & ~FILE_MASKS[7])
                               | ((extremeFwd(nonFileH) << 1) & ~FILE_MASKS[0]);
        return ~attacks;
    };

    const uint64_t whiteHoles = buildHoles(whitePawns, WHITE_FORWARD_FILL, true)  & CENTER_EXTENDED;
    const uint64_t blackHoles = buildHoles(blackPawns, BLACK_FORWARD_FILL, false) & CENTER_EXTENDED;

    const int whiteHoleCount = std::popcount(whiteHoles);
    const int blackHoleCount = std::popcount(blackHoles);

    constexpr int32_t HOLE_PENALTY = 4;
    int32_t score = (blackHoleCount - whiteHoleCount) * HOLE_PENALTY;

    const bool whiteBishopOnDark  = (b.bishops_bb[0] & DARK_SQUARES)  != 0ULL;
    const bool whiteBishopOnLight = (b.bishops_bb[0] & LIGHT_SQUARES) != 0ULL;
    const bool blackBishopOnDark  = (b.bishops_bb[1] & DARK_SQUARES)  != 0ULL;
    const bool blackBishopOnLight = (b.bishops_bb[1] & LIGHT_SQUARES) != 0ULL;

    if (whiteBishopOnDark)
        score += std::popcount(blackPawns & DARK_SQUARES)  * COLOR_COMPLEX_PENALTY;
    if (whiteBishopOnLight)
        score += std::popcount(blackPawns & LIGHT_SQUARES) * COLOR_COMPLEX_PENALTY;
    if (blackBishopOnDark)
        score -= std::popcount(whitePawns & DARK_SQUARES)  * COLOR_COMPLEX_PENALTY;
    if (blackBishopOnLight)
        score -= std::popcount(whitePawns & LIGHT_SQUARES) * COLOR_COMPLEX_PENALTY;

    return PhaseValue{score, score};
}

} // namespace engine
