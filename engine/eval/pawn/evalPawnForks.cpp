#include "../evaluator.hpp"

namespace engine {

static inline int32_t evalPawnForksSide(const chess::Board& b, int side) noexcept {
    const int opp = 1 - side;
    const uint64_t ownPawns = b.pawns_bb[side];
    const uint64_t enemyPieces = b.knights_bb[opp] | b.bishops_bb[opp] | b.rooks_bb[opp] | b.queens_bb[opp] | b.kings_bb[opp];

    int32_t score = 0;
    uint64_t pawns = ownPawns;

    while (pawns) {
        const uint8_t sq = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        // Only consider pawns defended by another friendly pawn
        if (!(pieces::PAWN_ATTACKERS_TO[side][sq] & ownPawns))
            continue;

        const uint64_t attacks = pieces::PAWN_ATTACKS[side][sq];
        const uint64_t forked  = attacks & enemyPieces;
        const int forkedCount  = __builtin_popcountll(forked);

        if (forkedCount < 2)
            continue;

        score += PAWN_FORK_BASE_BONUS;

        if (forked & (b.rooks_bb[opp] | b.queens_bb[opp]))
            score += PAWN_FORK_MAJOR_BONUS;

        if (forked & b.kings_bb[opp])
            score += PAWN_FORK_ROYAL_BONUS;
    }

    return score;
}

int32_t Evaluator::evalPawnForks(const chess::Board& b) noexcept {
    return evalPawnForksSide(b, 1) - evalPawnForksSide(b, 0);
}

} // namespace engine
