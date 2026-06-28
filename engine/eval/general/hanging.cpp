#include <bit>
#include "../evaluator.hpp"

namespace engine {

inline PhaseValue Evaluator::evalHangingPiecePenalty(uint64_t pieces, uint64_t enemyAttacks, uint64_t friendlyDef,
                                                      int sign, PhaseValue penalty) noexcept {
    const uint64_t hanging = pieces & enemyAttacks & ~friendlyDef;
    return (sign * static_cast<int32_t>(std::popcount(hanging))) * penalty;
}

inline PhaseValue Evaluator::evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side) noexcept {
    PhaseValue score{};
    const int opp  = side ^ 1;
    const int sign = (side == 0) ? 1 : -1;

    const uint64_t enemyAttacks = data[opp].allAttacks;
    const uint64_t friendlyDef = data[side].allAttacks;
    const uint64_t hangingPawns = b.pawns_bb[side] & enemyAttacks & ~friendlyDef;

    score += (sign * static_cast<int32_t>(std::popcount(hangingPawns))) * engine::HANGING_PAWN_PENALTY;

    const uint64_t kingBB = b.kings_bb[side];
    if (kingBB) [[likely]] {
        const int kingSq = std::countr_zero(kingBB);
        const uint64_t kingProximity = KING_PROXIMITY_MASKS[kingSq];
        const uint64_t criticalHangingPawns = hangingPawns & kingProximity;
        score += (sign * static_cast<int32_t>(std::popcount(criticalHangingPawns))) * engine::HANGING_PAWN_NEAR_KING_PENALTY;

        const uint64_t hookFiles = FILE_MASKS[1] | FILE_MASKS[6];
        const uint64_t hangingHookPawns = criticalHangingPawns & hookFiles;
        score += (sign * static_cast<int32_t>(std::popcount(hangingHookPawns))) * engine::HANGING_HOOK_PAWN_PENALTY;
    }

    score += evalHangingPiecePenalty(b.knights_bb[side] | b.bishops_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_MINOR_PENALTY);
    score += evalHangingPiecePenalty(b.rooks_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_ROOK_PENALTY);
    score += evalHangingPiecePenalty(b.queens_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_QUEEN_PENALTY);

    return score;
}

PhaseValue Evaluator::evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept {
    return evalHangingPiecesSide(b, data, 0) + evalHangingPiecesSide(b, data, 1);
}

uint64_t Evaluator::collectPawnAttacks(uint64_t pawns, int side) noexcept {
    if (side == 0) {
        return ((pawns >> 7) & ~FILE_MASKS[0]) | ((pawns >> 9) & ~FILE_MASKS[7]);
    }
    return ((pawns << 9) & ~FILE_MASKS[0]) | ((pawns << 7) & ~FILE_MASKS[7]);
}

uint64_t Evaluator::collectPawnPushAttacks(uint64_t pawns, int side, uint64_t occ) noexcept {
    const uint64_t empty = ~occ;
    if (side == 0) {
        const uint64_t pushed = (pawns >> 8) & empty;
        return ((pushed >> 7) & ~FILE_MASKS[0]) | ((pushed >> 9) & ~FILE_MASKS[7]);
    }
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

inline PhaseValue Evaluator::evalThreatsSide(const chess::Board& b, const AttackData data[2], int side,
                                              uint64_t occ) noexcept {
    const int opp = side ^ 1;
    const int sign = (side == 0) ? 1 : -1;
    const uint64_t ownMinors = b.knights_bb[side] | b.bishops_bb[side];
    const uint64_t ownRooks = b.rooks_bb[side];
    const uint64_t ownQueens = b.queens_bb[side];
    const uint64_t ownValuable = ownMinors | ownRooks | ownQueens;
    if (ownValuable == 0ULL) return {};

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

    PhaseValue score{};
    score += (sign * static_cast<int32_t>(std::popcount(ownMinors & pawnThreats))) * engine::THREAT_PAWN_ATTACK_MINOR_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(rookByPawn))) * engine::THREAT_PAWN_ATTACK_ROOK_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(rookByMinor))) * engine::THREAT_MINOR_ATTACK_ROOK_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(queenByPawn))) * engine::THREAT_PAWN_ATTACK_QUEEN_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(queenByMinor))) * engine::THREAT_MINOR_ATTACK_QUEEN_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(queenByRook))) * engine::THREAT_ROOK_ATTACK_QUEEN_PENALTY;

    score += (sign * static_cast<int32_t>(std::popcount(ownMinors & pawnPushThreats))) * engine::THREAT_PAWN_PUSH_MINOR_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(ownRooks & pawnPushThreats))) * engine::THREAT_PAWN_PUSH_ROOK_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(ownQueens & pawnPushThreats))) * engine::THREAT_PAWN_PUSH_QUEEN_PENALTY;

    const uint64_t looseValuable = ownValuable & data[opp].allAttacks & ~data[side].allAttacks;
    score += (sign * static_cast<int32_t>(std::popcount(looseValuable & ownMinors))) * engine::THREAT_LOOSE_MINOR_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(looseValuable & ownRooks))) * engine::THREAT_LOOSE_ROOK_PENALTY;
    score += (sign * static_cast<int32_t>(std::popcount(looseValuable & ownQueens))) * engine::THREAT_LOOSE_QUEEN_PENALTY;

    return score;
}

PhaseValue Evaluator::evalThreatsPair(const chess::Board& b, const AttackData data[2], uint64_t occ) noexcept {
    return evalThreatsSide(b, data, 0, occ) + evalThreatsSide(b, data, 1, occ);
}

} // namespace engine
