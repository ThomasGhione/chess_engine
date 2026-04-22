#include "../evaluator.hpp"

namespace engine {

inline int32_t Evaluator::evalHangingPiecePenalty(uint64_t pieces, uint64_t enemyAttacks, uint64_t friendlyDef, int sign, int penalty) noexcept {
    const uint64_t hanging = pieces & enemyAttacks & ~friendlyDef;
    return sign * __builtin_popcountll(hanging) * penalty;
}

inline int32_t Evaluator::evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept {
    int32_t score = 0;
    const int opp  = side ^ 1;

    const uint64_t enemyAttacks = data[opp].allAttacks;
    const uint64_t friendlyDef = data[side].allAttacks;
    const uint64_t hangingPawns = b.pawns_bb[side] & enemyAttacks & ~friendlyDef;

    score += sign * __builtin_popcountll(hangingPawns) * engine::HANGING_PAWN_PENALTY;

    const uint64_t kingBB = b.kings_bb[side];
    if (kingBB) [[likely]] {
        const int kingSq = __builtin_ctzll(kingBB);
        const uint64_t kingProximity = KING_PROXIMITY_MASKS[kingSq];
        const uint64_t criticalHangingPawns = hangingPawns & kingProximity;
        score += sign * __builtin_popcountll(criticalHangingPawns) * engine::HANGING_PAWN_NEAR_KING_PENALTY;

        const uint64_t hookFiles = FILE_MASKS[1] | FILE_MASKS[6];
        const uint64_t hangingHookPawns = criticalHangingPawns & hookFiles;
        score += sign * __builtin_popcountll(hangingHookPawns) * engine::HANGING_HOOK_PAWN_PENALTY;
    }

    score += evalHangingPiecePenalty(b.knights_bb[side] | b.bishops_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_MINOR_PENALTY);
    score += evalHangingPiecePenalty(b.rooks_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_ROOK_PENALTY);
    score += evalHangingPiecePenalty(b.queens_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_QUEEN_PENALTY);

    return score;
}

int32_t Evaluator::evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept {
    const int32_t scoreWhite = evalHangingPiecesSide(b, data, 0, 1);
    const int32_t scoreBlack = evalHangingPiecesSide(b, data, 1, -1);
    return scoreBlack + scoreWhite;
}

} // namespace engine
