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

uint64_t Evaluator::collectPawnAttacks(uint64_t pawns, int side) noexcept {
    if (side == 0) {
        // White attacks: rank decreases → shift right; file±1
        return ((pawns >> 7) & ~FILE_MASKS[0]) | ((pawns >> 9) & ~FILE_MASKS[7]);
    }
    // Black attacks: rank increases → shift left; file±1
    return ((pawns << 9) & ~FILE_MASKS[0]) | ((pawns << 7) & ~FILE_MASKS[7]);
}

uint64_t Evaluator::collectPawnPushAttacks(uint64_t pawns, int side, uint64_t occ) noexcept {
    const uint64_t empty = ~occ;
    if (side == 0) {
        // White: push decreases rank (>> 8), then attack
        const uint64_t pushed = (pawns >> 8) & empty;
        return ((pushed >> 7) & ~FILE_MASKS[0]) | ((pushed >> 9) & ~FILE_MASKS[7]);
    }
    // Black: push increases rank (<< 8), then attack
    const uint64_t pushed = (pawns << 8) & empty;
    return ((pushed << 9) & ~FILE_MASKS[0]) | ((pushed << 7) & ~FILE_MASKS[7]);
}

template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
uint64_t Evaluator::collectPieceAttacks(uint64_t piecesBb, uint64_t occ) noexcept {
    uint64_t attacks = 0ULL;
    while (piecesBb) {
        attacks |= AttackFn(popLSB(piecesBb), occ);
    }
    return attacks;
}

inline int32_t Evaluator::evalThreatsSide(const chess::Board& b, const AttackData data[2], int side,
                                          int sign, uint64_t occ) noexcept {
    const int opp = side ^ 1;
    const uint64_t ownMinors = b.knights_bb[side] | b.bishops_bb[side];
    const uint64_t ownRooks = b.rooks_bb[side];
    const uint64_t ownQueens = b.queens_bb[side];
    const uint64_t ownValuable = ownMinors | ownRooks | ownQueens;
    if (ownValuable == 0ULL) return 0;

    const uint64_t pawnThreats = collectPawnAttacks(b.pawns_bb[opp], opp);
    const uint64_t minorThreats = collectPieceAttacks<knightAttacksLookup>(b.knights_bb[opp], occ) 
                                | collectPieceAttacks<pieces::getBishopAttacks>(b.bishops_bb[opp], occ);
    const uint64_t rookThreats = collectPieceAttacks<pieces::getRookAttacks>(b.rooks_bb[opp], occ);
    const uint64_t pawnPushThreats = collectPawnPushAttacks(b.pawns_bb[opp], opp, occ);

    const uint64_t rookByPawn = ownRooks & pawnThreats;
    const uint64_t rookByMinor = ownRooks & minorThreats & ~rookByPawn;
    const uint64_t queenByPawn = ownQueens & pawnThreats;
    const uint64_t queenByMinor = ownQueens & minorThreats & ~queenByPawn;
    const uint64_t queenByRook = ownQueens & rookThreats & ~(queenByPawn | queenByMinor);

    int32_t score = 0;
    score += sign * __builtin_popcountll(ownMinors & pawnThreats) * engine::THREAT_PAWN_ATTACK_MINOR_PENALTY;
    score += sign * __builtin_popcountll(rookByPawn) * engine::THREAT_PAWN_ATTACK_ROOK_PENALTY;
    score += sign * __builtin_popcountll(rookByMinor) * engine::THREAT_MINOR_ATTACK_ROOK_PENALTY;
    score += sign * __builtin_popcountll(queenByPawn) * engine::THREAT_PAWN_ATTACK_QUEEN_PENALTY;
    score += sign * __builtin_popcountll(queenByMinor) * engine::THREAT_MINOR_ATTACK_QUEEN_PENALTY;
    score += sign * __builtin_popcountll(queenByRook) * engine::THREAT_ROOK_ATTACK_QUEEN_PENALTY;

    score += sign * __builtin_popcountll(ownMinors & pawnPushThreats) * engine::THREAT_PAWN_PUSH_MINOR_PENALTY;
    score += sign * __builtin_popcountll(ownRooks & pawnPushThreats) * engine::THREAT_PAWN_PUSH_ROOK_PENALTY;
    score += sign * __builtin_popcountll(ownQueens & pawnPushThreats) * engine::THREAT_PAWN_PUSH_QUEEN_PENALTY;

    const uint64_t looseValuable = ownValuable & data[opp].allAttacks & ~data[side].allAttacks;
    score += sign * __builtin_popcountll(looseValuable & ownMinors) * engine::THREAT_LOOSE_MINOR_PENALTY;
    score += sign * __builtin_popcountll(looseValuable & ownRooks) * engine::THREAT_LOOSE_ROOK_PENALTY;
    score += sign * __builtin_popcountll(looseValuable & ownQueens) * engine::THREAT_LOOSE_QUEEN_PENALTY;

    return score;
}

int32_t Evaluator::evalThreats(const chess::Board& b, const AttackData data[2], uint64_t occ, bool isEndgame) noexcept {
    int32_t score = evalThreatsSide(b, data, 0, 1, occ)
                  + evalThreatsSide(b, data, 1, -1, occ);

    if (isEndgame) {
        score = (score * engine::ENDGAME_THREAT_SCALE_PERCENT) / 100;
    }
    return score;
}

} // namespace engine
