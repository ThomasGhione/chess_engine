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

int64_t Evaluator::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    // NOTE: This function needs per-piece mobility, not aggregate mobility from AttackData
    // We still need to iterate through individual pieces to check if each one is trapped
    int64_t score = 0;

    // Small extra penalty to make truly trapped pieces slightly worse than the
    // base PINNED_* penalties. This keeps tuning local to the evaluation
    // function and avoids changing global constants.
    constexpr int64_t TRAPPED_EXTRA_SEVERITY = 10; // in centipawns

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;

        // Knights: use a precomputed lookup table (no magic bitboards)
        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = popLSB(knights);
            const int mobility = __builtin_popcountll((pieces::KNIGHT_ATTACKS[sq]) & ~occ);
            if (mobility == 0) [[unlikely]] score -= sign * (PINNED_KNIGHT_PENALTY + TRAPPED_EXTRA_SEVERITY);
            else if (mobility <= 3) score -= sign * LOW_MOBILITY_KNIGHT_PENALTY;
        }

        // Bishops, Rooks, Queens: calcola solo se pochi pezzi (risparmia magic bitboard lookups)
        const int pieceCount = __builtin_popcountll(b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]);
        
        if (pieceCount > 0) [[likely]] {
            // Bishops - magic bitboards
            uint64_t bishops = b.bishops_bb[side];
            while (bishops) {
                const int sq = popLSB(bishops);
                const uint64_t attacks = pieces::getBishopAttacks(sq, occ);
                const uint64_t ownOcc = (side == 0) ? (b.pawns_bb[0] | b.knights_bb[0] | b.bishops_bb[0] | b.rooks_bb[0] | b.queens_bb[0] | b.kings_bb[0])
                                         : (b.pawns_bb[1] | b.knights_bb[1] | b.bishops_bb[1] | b.rooks_bb[1] | b.queens_bb[1] | b.kings_bb[1]);
                const int mobility = __builtin_popcountll(attacks & ~ownOcc);
                if (mobility == 0) [[unlikely]] score -= sign * (PINNED_BISHOP_PENALTY + TRAPPED_EXTRA_SEVERITY);
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_BISHOP_PENALTY;
            }

            // Rooks - magic bitboards
            uint64_t rooks = b.rooks_bb[side];
            while (rooks) {
                const int sq = popLSB(rooks);
                const uint64_t attacks = pieces::getRookAttacks(sq, occ);
                const uint64_t ownOcc = (side == 0) ? (b.pawns_bb[0] | b.knights_bb[0] | b.bishops_bb[0] | b.rooks_bb[0] | b.queens_bb[0] | b.kings_bb[0])
                                         : (b.pawns_bb[1] | b.knights_bb[1] | b.bishops_bb[1] | b.rooks_bb[1] | b.queens_bb[1] | b.kings_bb[1]);
                const int mobility = __builtin_popcountll(attacks & ~ownOcc);
                if (mobility == 0) [[unlikely]] score -= sign * (PINNED_ROOK_PENALTY + TRAPPED_EXTRA_SEVERITY);
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_ROOK_PENALTY;
            }

            // Queens - magic bitboards
            uint64_t queens = b.queens_bb[side];
            while (queens) {
                const int sq = popLSB(queens);
                const uint64_t attacks = pieces::getQueenAttacks(sq, occ);
                const uint64_t ownOcc = (side == 0) ? (b.pawns_bb[0] | b.knights_bb[0] | b.bishops_bb[0] | b.rooks_bb[0] | b.queens_bb[0] | b.kings_bb[0])
                                         : (b.pawns_bb[1] | b.knights_bb[1] | b.bishops_bb[1] | b.rooks_bb[1] | b.queens_bb[1] | b.kings_bb[1]);
                const int mobility = __builtin_popcountll(attacks & ~ownOcc);
                if (mobility == 0) [[unlikely]] score -= sign * (PINNED_QUEEN_PENALTY + TRAPPED_EXTRA_SEVERITY);
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_QUEEN_PENALTY;
            }
        }
    }

    return score;
}

int64_t Evaluator::evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        const int opp  = side ^ 1;

        // Use precomputed attack maps!
        uint64_t enemyAttacks = data[opp].allAttacks;
        uint64_t friendlyDef = data[side].allAttacks;

        // Hanging pieces (attacked but undefended)
        // IMPORTANTE: i penalty sono già negativi, quindi usiamo += con sign
        uint64_t hanging;

        hanging = b.pawns_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_PAWN_PENALTY;

        hanging = b.knights_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_MINOR_PENALTY;

        hanging = b.bishops_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_MINOR_PENALTY;

        hanging = b.rooks_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_ROOK_PENALTY;

        hanging = b.queens_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_QUEEN_PENALTY;
    }

    return score;
}

} // namespace engine
