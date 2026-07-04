#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalInitiativePair(const chess::Board& b) noexcept {
    const int sign = (b.getActiveColor() == chess::Board::WHITE) ? 1 : -1;
    return sign * engine::INIT_BONUS;
}

} // namespace engine
