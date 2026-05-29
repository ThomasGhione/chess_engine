#include <bit>
#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalWeakSquares(const chess::Board& b,
                                       uint64_t whitePawns,
                                       uint64_t blackPawns) noexcept {
    constexpr uint64_t CENTER_EXTENDED = 0x00003C3C3C3C0000ULL;

    auto buildHoles = [](uint64_t ownPawns,
                         const std::array<uint64_t, 64>& fwd) -> uint64_t {
        uint64_t attacks = 0ULL;
        uint64_t p = ownPawns;
        while (p) {
            const int sq = std::countr_zero(p);
            p &= p - 1;
            const int file = chess::Board::file(sq);
            const uint64_t fileFwd = fwd[sq];
            if (file > 0) attacks |= (fileFwd >> 1) & ~FILE_MASKS[7];
            if (file < 7) attacks |= (fileFwd << 1) & ~FILE_MASKS[0];
        }
        return ~attacks;
    };

    const uint64_t whiteHoles = buildHoles(whitePawns, WHITE_FORWARD_FILL) & CENTER_EXTENDED;
    const uint64_t blackHoles = buildHoles(blackPawns, BLACK_FORWARD_FILL) & CENTER_EXTENDED;

    const int whiteHoleCount = std::popcount(whiteHoles);
    const int blackHoleCount = std::popcount(blackHoles);

    constexpr int32_t HOLE_PENALTY = 4;
    int32_t score = (blackHoleCount - whiteHoleCount) * HOLE_PENALTY;

    const bool whiteBishopOnDark  = (b.bishops_bb[0] & DARK_SQUARES)  != 0ULL;
    const bool whiteBishopOnLight = (b.bishops_bb[0] & LIGHT_SQUARES) != 0ULL;
    const bool blackBishopOnDark  = (b.bishops_bb[1] & DARK_SQUARES)  != 0ULL;
    const bool blackBishopOnLight = (b.bishops_bb[1] & LIGHT_SQUARES) != 0ULL;

    constexpr int32_t COLOR_COMPLEX_PENALTY = 2;

    if (whiteBishopOnDark)
        score -= std::popcount(blackPawns & DARK_SQUARES)  * COLOR_COMPLEX_PENALTY;
    if (whiteBishopOnLight)
        score -= std::popcount(blackPawns & LIGHT_SQUARES) * COLOR_COMPLEX_PENALTY;
    if (blackBishopOnDark)
        score += std::popcount(whitePawns & DARK_SQUARES)  * COLOR_COMPLEX_PENALTY;
    if (blackBishopOnLight)
        score += std::popcount(whitePawns & LIGHT_SQUARES) * COLOR_COMPLEX_PENALTY;

    return PhaseValue{score, score};
}

} // namespace engine
