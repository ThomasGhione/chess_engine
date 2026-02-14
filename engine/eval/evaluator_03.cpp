#include "evaluator.hpp"
#include "../piecevaluetables.hpp"
#include <algorithm>
#include <cstring>
namespace engine {

int64_t Evaluator::evalMinorPieceDevelopment(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL; // rank 1 & 2 (bit 56-63 + 48-55)
    static constexpr uint64_t BLACK_MINOR_START = 0x000000000000FFFFULL; // rank 8 & 7 (bit 0-7 + 8-15)
    
    // Conta pezzi sviluppati (fuori dalle caselle iniziali) in una sola operazione
    const int whiteDeveloped = 
        __builtin_popcountll(b.knights_bb[0] & ~WHITE_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[0] & ~WHITE_MINOR_START);
    
    const int blackDeveloped = 
        __builtin_popcountll(b.knights_bb[1] & ~BLACK_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[1] & ~BLACK_MINOR_START);
    
    return (whiteDeveloped - blackDeveloped) * DEVELOPMENT_BONUS;
}

int64_t Evaluator::evalEarlyQueen(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_QUEEN_START = chess::Board::bitMask(59); // d1
    static constexpr uint64_t BLACK_QUEEN_START = chess::Board::bitMask(3);  // d8
    static constexpr int64_t EARLY_QUEEN_DEV_PENALTY = 20;
    
    int64_t score = 0;

    score -= (b.queens_bb[0] && !(b.queens_bb[0] & WHITE_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;
    score += (b.queens_bb[1] && !(b.queens_bb[1] & BLACK_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;

    return score;
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
    const uint64_t ownOcc = (side == 0)
        ? (b.pawns_bb[0] | b.knights_bb[0] | b.bishops_bb[0] | b.rooks_bb[0] | b.queens_bb[0] | b.kings_bb[0])
        : (b.pawns_bb[1] | b.knights_bb[1] | b.bishops_bb[1] | b.rooks_bb[1] | b.queens_bb[1] | b.kings_bb[1]);

    sideScore += evalTrappedPiecesGeneric<knightAttacksLookup, PINNED_KNIGHT_PENALTY, LOW_MOBILITY_KNIGHT_PENALTY>(
        b.knights_bb[side], occ, ~occ, sign);

    // Bishops, Rooks, Queens: calcola solo se pochi pezzi (risparmia magic bitboard lookups)
    const int pieceCount = __builtin_popcountll(b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]);
    
    if (pieceCount <= 0) [[unlikely]] {
      return sideScore;
    }

    sideScore += evalTrappedPiecesGeneric<pieces::getBishopAttacks, PINNED_BISHOP_PENALTY, LOW_MOBILITY_BISHOP_PENALTY>(
        b.bishops_bb[side], occ, ~ownOcc, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getRookAttacks, PINNED_ROOK_PENALTY, LOW_MOBILITY_ROOK_PENALTY>(
        b.rooks_bb[side], occ, ~ownOcc, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getQueenAttacks, PINNED_QUEEN_PENALTY, LOW_MOBILITY_QUEEN_PENALTY>(
        b.queens_bb[side], occ, ~ownOcc, sign);

    return sideScore;
}

int64_t Evaluator::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    // NOTE: This function needs per-piece mobility, not aggregate mobility from AttackData
    // We still need to iterate through individual pieces to check if each one is trapped
    int64_t score = 0;

    score += evalTrappedPiecesSide(b, occ, 0, 1);
    score += evalTrappedPiecesSide(b, occ, 1, -1);

    return score;
}

} // namespace engine
