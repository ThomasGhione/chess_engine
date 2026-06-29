#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalMinorPieceDevelopment(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL;
    static constexpr uint64_t BLACK_MINOR_START = 0x00000000000000FFULL;

    const int whiteDeveloped =
        std::popcount(b.knights_bb[0] & ~WHITE_MINOR_START) +
        std::popcount(b.bishops_bb[0] & ~WHITE_MINOR_START);

    const int blackDeveloped =
        std::popcount(b.knights_bb[1] & ~BLACK_MINOR_START) +
        std::popcount(b.bishops_bb[1] & ~BLACK_MINOR_START);

    return (whiteDeveloped - blackDeveloped) * engine::DEVELOPMENT_BONUS;
}

} // namespace engine
