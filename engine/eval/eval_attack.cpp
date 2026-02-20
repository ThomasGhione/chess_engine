#include "evaluator.hpp"
#include <cstring>

namespace engine {

void Evaluator::computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
    std::memset(data, 0, 2 * sizeof(AttackData));

    for (int side = 0; side < 2; ++side) {
        AttackData& d = data[side];
        const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                                b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];

        uint64_t pawns = b.pawns_bb[side];
        const bool isWhite = (side == 0);
        while (pawns) {
            const int sq = popLSB(pawns);
            d.pawnAttacks |= pieces::PAWN_ATTACKS[isWhite][sq];
        }
        d.allAttacks = d.pawnAttacks;

        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = popLSB(knights);
            const uint64_t attacks = pieces::KNIGHT_ATTACKS[sq];
            d.knightAttacks |= attacks;
            const int mobility = __builtin_popcountll(attacks & ~ownOcc);
            d.knightMobility += mobility;
        }
        d.allAttacks |= d.knightAttacks;

        uint64_t bishops = b.bishops_bb[side];
        while (bishops) {
            const int sq = popLSB(bishops);
            const uint64_t attacks = pieces::getBishopAttacks(sq, occ);
            d.bishopAttacks |= attacks;
            d.bishopMobility += __builtin_popcountll(attacks & ~ownOcc);
        }
        d.allAttacks |= d.bishopAttacks;

        uint64_t rooks = b.rooks_bb[side];
        while (rooks) {
            const int sq = popLSB(rooks);
            const uint64_t attacks = pieces::getRookAttacks(sq, occ);
            d.rookAttacks |= attacks;
            d.rookMobility += __builtin_popcountll(attacks & ~ownOcc);
        }
        d.allAttacks |= d.rookAttacks;

        uint64_t queens = b.queens_bb[side];
        while (queens) {
            const int sq = popLSB(queens);
            const uint64_t attacks = pieces::getQueenAttacks(sq, occ);
            d.queenAttacks |= attacks;
            d.queenMobility += __builtin_popcountll(attacks & ~ownOcc);
        }
        d.allAttacks |= d.queenAttacks;

        d.isComputed = true;
    }
}

int64_t Evaluator::evalMobility(const AttackData data[2]) noexcept {
    return (data[0].knightMobility + data[0].bishopMobility + data[0].rookMobility + data[0].queenMobility
          - data[1].knightMobility - data[1].bishopMobility - data[1].rookMobility - data[1].queenMobility) / 2;
}

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

inline uint64_t Evaluator::knightAttacksLookup(uint8_t sq, uint64_t) noexcept {
    return pieces::KNIGHT_ATTACKS[sq];
}

template<uint64_t (*AttackFn)(uint8_t, uint64_t), int64_t PinnedPenalty, int64_t LowMobPenalty>
inline int64_t Evaluator::evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask, int sign) noexcept {
    int64_t score = 0;
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        const uint64_t attacks = AttackFn(sq, occ);
        const int mobility = __builtin_popcountll(attacks & mobilityMask);
        if (mobility == 0) [[unlikely]] score -= sign * (PinnedPenalty + TRAPPED_EXTRA_SEVERITY);
        else if (mobility <= 3) score -= sign * LowMobPenalty;
    }
    return score;
}

inline int64_t Evaluator::evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side, int sign) noexcept {
    int64_t sideScore = 0;
    const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                            b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];

    sideScore += evalTrappedPiecesGeneric<knightAttacksLookup, PINNED_KNIGHT_PENALTY, LOW_MOBILITY_KNIGHT_PENALTY>(
        b.knights_bb[side], occ, ~occ, sign);

    const int pieceCount = __builtin_popcountll(b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]);

    if (pieceCount <= 0) [[unlikely]] {
      return sideScore;
    }

    sideScore += evalTrappedPiecesGeneric<pieces::getBishopAttacks, engine::PINNED_BISHOP_PENALTY, engine::LOW_MOBILITY_BISHOP_PENALTY>(
        b.bishops_bb[side], occ, ~ownOcc, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getRookAttacks, engine::PINNED_ROOK_PENALTY, engine::LOW_MOBILITY_ROOK_PENALTY>(
        b.rooks_bb[side], occ, ~ownOcc, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getQueenAttacks, engine::PINNED_QUEEN_PENALTY, engine::LOW_MOBILITY_QUEEN_PENALTY>(
        b.queens_bb[side], occ, ~ownOcc, sign);

    return sideScore;
}

int64_t Evaluator::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;
    for (int side = 0; side < 2; ++side) {
        score += evalTrappedPiecesSide(b, occ, side, (side == 0) ? 1 : -1);
    }
    return score;
}

} // namespace engine
