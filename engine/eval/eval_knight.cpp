#include "evaluator.hpp"

namespace engine {

int32_t Evaluator::evalMinorPieceDevelopment(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL;
    static constexpr uint64_t BLACK_MINOR_START = 0x000000000000FFFFULL;

    const int whiteDeveloped =
        __builtin_popcountll(b.knights_bb[0] & ~WHITE_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[0] & ~WHITE_MINOR_START);

    const int blackDeveloped =
        __builtin_popcountll(b.knights_bb[1] & ~BLACK_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[1] & ~BLACK_MINOR_START);

    return (whiteDeveloped - blackDeveloped) * engine::DEVELOPMENT_BONUS;
}

} // namespace engine
