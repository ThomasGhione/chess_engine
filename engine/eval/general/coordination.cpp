#include "../evaluator.hpp"

namespace engine {

inline PhaseValue Evaluator::evalOutpostsPieces(uint64_t piecesBb, int color,
                                                 const chess::Board& b, PhaseValue bonus) noexcept {
    const int opp  = color ^ 1;
    const int sign = (color == 0) ? 1 : -1;
    PhaseValue score{};
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[color][sq] & b.pawns_bb[color]) != 0;
        const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[opp][sq] & b.pawns_bb[opp]) != 0;
        if (supportedByPawn && !attackedByEnemyPawn) {
            score += sign * bonus;
        }
    }
    return score;
}

inline PhaseValue Evaluator::evalOutpostsForColor(const chess::Board& b, int color) noexcept {
    PhaseValue score{};

    score += evalOutpostsPieces(b.knights_bb[color], color, b, engine::OUTPOST_KNIGHT_BONUS);
    // Bishop bonus halved (preserves prior division by 2).
    score += evalOutpostsPieces(b.bishops_bb[color], color, b,
                                 PhaseValue{engine::OUTPOST_BISHOP_BONUS.mg / 2, engine::OUTPOST_BISHOP_BONUS.eg / 2});

    return score;
}

PhaseValue Evaluator::evalOutposts(const chess::Board& b) noexcept {
    return evalOutpostsForColor(b, 0) + evalOutpostsForColor(b, 1);
}

inline PhaseValue Evaluator::evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept {
    PhaseValue score{};
    const int sign = (color == 0) ? -1 : 1;

    uint64_t minors = b.knights_bb[color] | b.bishops_bb[color];
    const uint64_t friends = b.pawns_bb[color] | b.knights_bb[color] | b.bishops_bb[color] | b.rooks_bb[color] | b.queens_bb[color];
    while (minors) {
        const int sq = popLSB(minors);
        const uint64_t nearby = KING_PROXIMITY_MASKS[sq];
        if ((friends & nearby) == 0) {
            score += sign * engine::COORDINATION_PENALTY;
        }
    }
    return score;
}

PhaseValue Evaluator::evalPieceCoordination(const chess::Board& b) noexcept {
    return evalPieceCoordinationForColor(b, 0) + evalPieceCoordinationForColor(b, 1);
}

} // namespace engine
