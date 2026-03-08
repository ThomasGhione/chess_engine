#include "../evaluator.hpp"

namespace engine {

template<uint64_t (*AttackFn)(uint8_t, uint64_t), int32_t PinnedPenalty, int32_t LowMobPenalty>
inline int32_t Evaluator::evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask, int sign) noexcept {
    int32_t score = 0;
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        const uint64_t attacks = AttackFn(sq, occ);
        const int mobility = __builtin_popcountll(attacks & mobilityMask);
        if (mobility == 0) score -= sign * (PinnedPenalty + TRAPPED_EXTRA_SEVERITY);
        else if (mobility <= 3) score -= sign * LowMobPenalty;
    }
    return score;
}

inline int32_t Evaluator::evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side, int sign) noexcept {
    int32_t sideScore = 0;
    const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                            b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];
    const uint64_t mobilityMask = ~occ;
    const uint64_t mobilityOwnMask = ~ownOcc;

    sideScore += evalTrappedPiecesGeneric<knightAttacksLookup, PINNED_KNIGHT_PENALTY, LOW_MOBILITY_KNIGHT_PENALTY>(
        b.knights_bb[side], occ, mobilityMask, sign);

    if ((b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]) == 0ULL) {
      return sideScore;
    }

    sideScore += evalTrappedPiecesGeneric<pieces::getBishopAttacks, engine::PINNED_BISHOP_PENALTY, engine::LOW_MOBILITY_BISHOP_PENALTY>(
        b.bishops_bb[side], occ, mobilityOwnMask, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getRookAttacks, engine::PINNED_ROOK_PENALTY, engine::LOW_MOBILITY_ROOK_PENALTY>(
        b.rooks_bb[side], occ, mobilityOwnMask, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getQueenAttacks, engine::PINNED_QUEEN_PENALTY, engine::LOW_MOBILITY_QUEEN_PENALTY>(
        b.queens_bb[side], occ, mobilityOwnMask, sign);

    return sideScore;
}

int32_t Evaluator::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    int32_t score = 0;
    for (int side = 0; side < 2; ++side) {
        score += evalTrappedPiecesSide(b, occ, side, (side == 0) ? 1 : -1);
    }
    return score;
}

} // namespace engine
