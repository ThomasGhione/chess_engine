#include <bit>
#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    static constexpr uint64_t CENTER_MASK = 0x0000001818000000ULL;
    return (std::popcount(whitePawns & CENTER_MASK) - std::popcount(blackPawns & CENTER_MASK)) * engine::CENTER_CONTROL_BONUS;
}

} // namespace engine

