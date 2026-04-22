#include "../evaluator.hpp"

namespace engine {

template<int32_t Bonus>
inline int32_t Evaluator::evalOutpostsPieces(uint64_t piecesBb, int color, int opp, int sign, const chess::Board& b) noexcept {
    int32_t score = 0;
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[color][sq] & b.pawns_bb[color]) != 0;
        const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[opp][sq] & b.pawns_bb[opp]) != 0;
        
        score += (supportedByPawn && !attackedByEnemyPawn) * sign * Bonus;
    }
    return score;
}

inline int32_t Evaluator::evalOutpostsForColor(const chess::Board& b, int color) noexcept {
    int32_t score = 0;
    const int sign = (color == 0) ? 1 : -1;
    const int opp = color ^ 1;

    score += evalOutpostsPieces<engine::OUTPOST_KNIGHT_BONUS>(b.knights_bb[color], color, opp, sign, b);
    score += evalOutpostsPieces<engine::OUTPOST_BISHOP_BONUS / 2>(b.bishops_bb[color], color, opp, sign, b);

    return score;
}

int32_t Evaluator::evalOutposts(const chess::Board& b) noexcept {
    return evalOutpostsForColor(b, 0) + evalOutpostsForColor(b, 1);
}

inline int32_t Evaluator::evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept {
    int32_t score = 0;
    const int sign = (color == 0) ? -1 : 1;

    uint64_t minors = b.knights_bb[color] | b.bishops_bb[color];
    const uint64_t friends = b.pawns_bb[color] | b.knights_bb[color] | b.bishops_bb[color] | b.rooks_bb[color] | b.queens_bb[color];
    while (minors) {
        const int sq = popLSB(minors);
        const uint64_t nearby = KING_PROXIMITY_MASKS[sq];
        score += ((friends & nearby) == 0) * sign * engine::COORDINATION_PENALTY;
    }
    return score;
}

int32_t Evaluator::evalPieceCoordination(const chess::Board& b) noexcept {
    return evalPieceCoordinationForColor(b, 0) + evalPieceCoordinationForColor(b, 1);
}

} // namespace engine
