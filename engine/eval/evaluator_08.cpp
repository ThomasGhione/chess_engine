#include "evaluator.hpp"

namespace engine {
int64_t Evaluator::evalMobility(const AttackData data[2]) noexcept {
    return (data[0].knightMobility + data[0].bishopMobility + data[0].rookMobility + data[0].queenMobility
          - data[1].knightMobility - data[1].bishopMobility - data[1].rookMobility - data[1].queenMobility) / 2;
}

int64_t Evaluator::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return (side == 0) ? Evaluator::evalBadBishopImpl<0>(bishops, pawns) : Evaluator::evalBadBishopImpl<1>(bishops, pawns);
}

} // namespace engine
