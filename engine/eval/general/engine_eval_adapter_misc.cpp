#include "../../engine.hpp"
#include "../evaluator.hpp"

namespace engine {

int32_t Engine::getMaterialDelta(const chess::Board& b) noexcept {
    return Evaluator::getMaterialDelta(b);
}

} // namespace engine
