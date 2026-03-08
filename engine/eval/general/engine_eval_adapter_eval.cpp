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

int32_t Engine::evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    return Evaluator::evalPawnStructure(whitePawns, blackPawns, isEndgame);
}

int32_t Engine::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return Evaluator::evalKingSafety(b, whitePawns, blackPawns);
}

} // namespace engine
