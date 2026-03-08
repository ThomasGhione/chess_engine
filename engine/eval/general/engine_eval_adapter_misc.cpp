#include "../../engine.hpp"
#include "../evaluator.hpp"

namespace engine {

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
