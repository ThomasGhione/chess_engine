#include "evaluator.hpp"

namespace engine {

template<int64_t Bonus>
inline int64_t Evaluator::evalOutpostsPieces(uint64_t piecesBb, int color, int opp, int sign, const chess::Board& b) noexcept {
    int64_t score = 0;
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[color][sq] & b.pawns_bb[color]) != 0;
        const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[opp][sq] & b.pawns_bb[opp]) != 0;
        if (supportedByPawn && !attackedByEnemyPawn) {
            score += sign * Bonus;
        }
    }
    return score;
}

inline int64_t Evaluator::evalOutpostsForColor(const chess::Board& b, int color) noexcept {
    int64_t score = 0;
    const int sign = (color == 0) ? 1 : -1;
    const int opp = color ^ 1;

    score += evalOutpostsPieces<engine::OUTPOST_KNIGHT_BONUS>(b.knights_bb[color], color, opp, sign, b);
    score += evalOutpostsPieces<engine::OUTPOST_BISHOP_BONUS / 2>(b.bishops_bb[color], color, opp, sign, b);

    return score;
}

int64_t Evaluator::evalOutposts(const chess::Board& b) noexcept {
    int64_t score = 0;
    for (int side = 0; side < 2; ++side) {
        score += evalOutpostsForColor(b, side);
    }
    return score;
}

inline int64_t Evaluator::evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept {
    int64_t score = 0;
    const int sign = (color == 0) ? -1 : 1;

    uint64_t minors = b.knights_bb[color] | b.bishops_bb[color];
    if (!minors) {
      return score;
    }

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

int64_t Evaluator::evalPieceCoordination(const chess::Board& b) noexcept {
    int64_t score = 0;
    for (int side = 0; side < 2; ++side) {
        score += evalPieceCoordinationForColor(b, side);
    }
    return score;
}

} // namespace engine
