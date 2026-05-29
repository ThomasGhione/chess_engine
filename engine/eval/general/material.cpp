#include "../evaluator.hpp"

namespace engine {


int32_t Evaluator::evalInitiative(const chess::Board& b, bool isEndgame) noexcept {
    return isEndgame
        ? evalInitiativeImpl<true>(b.getActiveColor())
        : evalInitiativeImpl<false>(b.getActiveColor());
}

PhaseValue Evaluator::evalInitiativePair(const chess::Board& b) noexcept {
    const int sign = (b.getActiveColor() == chess::Board::WHITE) ? 1 : -1;
    return PhaseValue{sign * INIT_BONUS_MG, sign * INIT_BONUS_EG};
}

} // namespace engine
