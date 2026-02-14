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

    score += evalOutpostsPieces<OUTPOST_KNIGHT_BONUS>(b.knights_bb[color], color, opp, sign, b);
    score += evalOutpostsPieces<OUTPOST_BISHOP_BONUS / 2>(b.bishops_bb[color], color, opp, sign, b);

    return score;
}

int64_t Evaluator::evalOutposts(const chess::Board& b) noexcept {
    const int64_t whiteOutpostScore = Evaluator::evalOutpostsForColor(b, 0);
    const int64_t blackOutpostScore = Evaluator::evalOutpostsForColor(b, 1);

    return whiteOutpostScore + blackOutpostScore;
}

} // namespace engine
