#include "evaluator.hpp"
namespace engine {
inline int64_t Evaluator::evalHangingPiecePenalty(uint64_t pieces, uint64_t enemyAttacks, uint64_t friendlyDef, int sign, int penalty) noexcept {
    const uint64_t hanging = pieces & enemyAttacks & ~friendlyDef;
    return sign * __builtin_popcountll(hanging) * penalty;
}

inline int64_t Evaluator::evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept {
    int64_t score = 0;
    const int opp  = side ^ 1;

    uint64_t enemyAttacks = data[opp].allAttacks;
    uint64_t friendlyDef = data[side].allAttacks;

    score += evalHangingPiecePenalty(b.pawns_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_PAWN_PENALTY);
    score += evalHangingPiecePenalty(b.knights_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_MINOR_PENALTY);
    score += evalHangingPiecePenalty(b.bishops_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_MINOR_PENALTY);
    score += evalHangingPiecePenalty(b.rooks_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_ROOK_PENALTY);
    score += evalHangingPiecePenalty(b.queens_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_QUEEN_PENALTY);

    return score;
}

int64_t Evaluator::evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept {
    const int64_t scoreWhite = evalHangingPiecesSide(b, data, 0, 1);
    const int64_t scoreBlack = evalHangingPiecesSide(b, data, 1, -1);

    return scoreBlack + scoreWhite;
}
} // namespace engine
