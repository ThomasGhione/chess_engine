#include "evaluator.hpp"

namespace engine {
int64_t Evaluator::evalMobility(const AttackData data[2]) noexcept {
    return (data[0].knightMobility + data[0].bishopMobility + data[0].rookMobility + data[0].queenMobility
          - data[1].knightMobility - data[1].bishopMobility - data[1].rookMobility - data[1].queenMobility) / 2;
}

int64_t Evaluator::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return (side == 0) ? Evaluator::evalBadBishopImpl<0>(bishops, pawns) : Evaluator::evalBadBishopImpl<1>(bishops, pawns);
}

inline int64_t Evaluator::evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept {
    int64_t score = 0;
    const int opp  = side ^ 1;

    uint64_t enemyAttacks = data[opp].allAttacks;
    uint64_t friendlyDef = data[side].allAttacks;

    uint64_t hanging = b.pawns_bb[side] & enemyAttacks & ~friendlyDef;
    score += sign * __builtin_popcountll(hanging) * engine::HANGING_PAWN_PENALTY;

    hanging = b.knights_bb[side] & enemyAttacks & ~friendlyDef;
    score += sign * __builtin_popcountll(hanging) * engine::HANGING_MINOR_PENALTY;

    hanging = b.bishops_bb[side] & enemyAttacks & ~friendlyDef;
    score += sign * __builtin_popcountll(hanging) * engine::HANGING_MINOR_PENALTY;

    hanging = b.rooks_bb[side] & enemyAttacks & ~friendlyDef;
    score += sign * __builtin_popcountll(hanging) * engine::HANGING_ROOK_PENALTY;

    hanging = b.queens_bb[side] & enemyAttacks & ~friendlyDef;
    score += sign * __builtin_popcountll(hanging) * engine::HANGING_QUEEN_PENALTY;

    return score;
}

int64_t Evaluator::evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept {
    const int64_t scoreWhite = evalHangingPiecesSide(b, data, 0, 1);
    const int64_t scoreBlack = evalHangingPiecesSide(b, data, 1, -1);

    return scoreBlack + scoreWhite;
}

} // namespace engine
