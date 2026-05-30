#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalInitiative(const chess::Board& b, bool /*isEndgame*/) noexcept {
    // Returns the full (mg, eg) initiative pair. The deprecated isEndgame flag
    // is preserved for compatibility with the perf benchmark suite; the unified
    // evaluator uses evalInitiativePair directly.
    return evalInitiativePair(b);
}

PhaseValue Evaluator::evalInitiativePair(const chess::Board& b) noexcept {
    const int sign = (b.getActiveColor() == chess::Board::WHITE) ? 1 : -1;
    return sign * engine::INIT_BONUS;
}

} // namespace engine
