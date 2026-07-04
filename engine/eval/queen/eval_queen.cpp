#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalEarlyQueen(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_QUEEN_START = chess::Board::BIT_MASKS[59];
    static constexpr uint64_t BLACK_QUEEN_START = chess::Board::BIT_MASKS[3];
    static constexpr int32_t EARLY_QUEEN_DEV_PENALTY = 20;

    int32_t score = 0;
    score -= (b.queens_bb[0] && !(b.queens_bb[0] & WHITE_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;
    score += (b.queens_bb[1] && !(b.queens_bb[1] & BLACK_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;

    return {score, 0};
}

inline PhaseValue Evaluator::evalQueenEndgamePressureSide(const chess::Board& b, int side, int ourQueens) noexcept {
    if (ourQueens == 0) return {};

    const int oppSide = side ^ 1;

    const int oppPawns = std::popcount(b.pawns_bb[oppSide]);
    const int oppMaterial = nonPawnMaterial(b, oppSide) + oppPawns * 100;
    if (oppMaterial > 1300) return {};

    const uint64_t enemyKingBB = b.kings_bb[oppSide];
    if (!enemyKingBB) return {};

    const int enemyKingSq = std::countr_zero(enemyKingBB);

    constexpr int32_t QUEEN_EG_EDGE_BONUS = 55;
    int32_t sideScore = edgeProximity(enemyKingSq) * QUEEN_EG_EDGE_BONUS;
    sideScore += ownKingProximity(b.kings_bb[side], enemyKingSq) * 14;

    uint64_t tempQueens = b.queens_bb[side];
    int bestQueenDist = 100;
    while (tempQueens) {
        bestQueenDist = std::min(bestQueenDist, manhattan(popLSB(tempQueens), enemyKingSq));
    }

    if (bestQueenDist >= 2 && bestQueenDist <= 5) {
        sideScore += 24;
    } else if (bestQueenDist <= 7) {
        sideScore += 10;
    }

    constexpr int32_t QUEEN_EG_PRESSURE_CAP = 180;
    sideScore = std::min(sideScore, QUEEN_EG_PRESSURE_CAP);

    const int sign = (side == 0) ? 1 : -1;
    return {0, sign * sideScore};
}

PhaseValue Evaluator::evalQueenEndgamePressure(const chess::Board& b) noexcept {
    const int whiteQueens = std::popcount(b.queens_bb[0]);
    const int blackQueens = std::popcount(b.queens_bb[1]);

    if (whiteQueens == 0 && blackQueens == 0) return {};

    return evalQueenEndgamePressureSide(b, 0, whiteQueens)
         + evalQueenEndgamePressureSide(b, 1, blackQueens);
}

} // namespace engine
