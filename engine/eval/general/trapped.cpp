#include "../evaluator.hpp"

namespace engine {

template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
inline PhaseValue Evaluator::evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask,
                                                       int sign, PhaseValue pinnedPenalty, PhaseValue lowMobPenalty) noexcept {
    PhaseValue score{};
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        const uint64_t attacks = AttackFn(sq, occ);
        const int mobility = std::popcount(attacks & mobilityMask);
        if (mobility == 0) {
            score -= sign * (pinnedPenalty + PhaseValue{TRAPPED_EXTRA_SEVERITY});
        } else if (mobility <= 3) {
            score -= sign * lowMobPenalty;
        }
    }
    return score;
}

inline PhaseValue Evaluator::evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side) noexcept {
    PhaseValue sideScore{};
    const int sign = (side == 0) ? 1 : -1;
    const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                            b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];
    const uint64_t mobilityOwnMask = ~ownOcc;

    sideScore += evalTrappedPiecesGeneric<knightAttacksLookup>(
        b.knights_bb[side], occ, mobilityOwnMask, sign, PINNED_KNIGHT_PENALTY, LOW_MOBILITY_KNIGHT_PENALTY);

    if ((b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]) == 0ULL) {
      return sideScore;
    }

    sideScore += evalTrappedPiecesGeneric<pieces::getBishopAttacks>(
        b.bishops_bb[side], occ, mobilityOwnMask, sign, engine::PINNED_BISHOP_PENALTY, engine::LOW_MOBILITY_BISHOP_PENALTY);

    sideScore += evalTrappedPiecesGeneric<pieces::getRookAttacks>(
        b.rooks_bb[side], occ, mobilityOwnMask, sign, engine::PINNED_ROOK_PENALTY, engine::LOW_MOBILITY_ROOK_PENALTY);

    sideScore += evalTrappedPiecesGeneric<pieces::getQueenAttacks>(
        b.queens_bb[side], occ, mobilityOwnMask, sign, engine::PINNED_QUEEN_PENALTY, engine::LOW_MOBILITY_QUEEN_PENALTY);

    return sideScore;
}

PhaseValue Evaluator::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    return evalTrappedPiecesSide(b, occ, 0) + evalTrappedPiecesSide(b, occ, 1);
}

} // namespace engine
