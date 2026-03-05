#include "../engine.hpp"
#include "evaluator.hpp"

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

int32_t Engine::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return Evaluator::evalRooks(whiteRooks, blackRooks, whitePawns, blackPawns);
}

int32_t Engine::evalKingActivity(const chess::Board& b, bool isEndgame) noexcept {
    return Evaluator::evalKingActivity(b, isEndgame);
}

int32_t Engine::evalEndgameKingActivity(const chess::Board& b) noexcept {
    return Evaluator::evalEndgameKingActivity(b);
}

int32_t Engine::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return Evaluator::evalBadBishop(bishops, pawns, side);
}

int32_t Engine::getMaterialDelta(const chess::Board& b) noexcept {
    return Evaluator::getMaterialDelta(b);
}

} // namespace engine
