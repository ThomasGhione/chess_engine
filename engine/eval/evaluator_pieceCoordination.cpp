#include "evaluator.hpp"
namespace engine {

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
    const int64_t whiteCoordinationScore = Evaluator::evalPieceCoordinationForColor(b, 0);
    const int64_t blackCoordinationScore = Evaluator::evalPieceCoordinationForColor(b, 1);

    return whiteCoordinationScore + blackCoordinationScore;
}

} // namespace engine
