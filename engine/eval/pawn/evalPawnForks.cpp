#include <bit>
#include "../evaluator.hpp"

namespace engine {

static inline PhaseValue evalPawnForksSide(const chess::Board& b, int side) noexcept {
    const int opp = 1 - side;
    const uint64_t ownPawns = b.pawns_bb[side];
    const uint64_t enemyPieces = b.knights_bb[opp] | b.bishops_bb[opp] | b.rooks_bb[opp] | b.queens_bb[opp] | b.kings_bb[opp];

    PhaseValue score{};
    uint64_t pawns = ownPawns;

    while (pawns) {
        const uint8_t sq = popLSB(pawns);

        if (!(pieces::PAWN_ATTACKERS_TO[side][sq] & ownPawns))
            continue;

        const uint64_t attacks = pieces::PAWN_ATTACKS[side][sq];
        const uint64_t forked  = attacks & enemyPieces;
        const int forkedCount  = std::popcount(forked);

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

PhaseValue Evaluator::evalPawnForks(const chess::Board& b) noexcept {
    return evalPawnForksSide(b, 0) - evalPawnForksSide(b, 1);
}

} // namespace engine
