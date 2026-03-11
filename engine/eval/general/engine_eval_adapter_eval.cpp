#include "../../engine.hpp"
#include "../evaluator.hpp"

namespace engine {

int32_t Engine::evaluate(const chess::Board& board) noexcept {
    return Evaluator::evaluate(board);
}

int32_t Engine::evaluateTrace(const chess::Board& board) noexcept {
    return Evaluator::evaluateTrace(board);
}

int32_t Engine::evaluateCheckmate(const chess::Board& board) noexcept {
    return Evaluator::evaluateCheckmate(board);
}

} // namespace engine
