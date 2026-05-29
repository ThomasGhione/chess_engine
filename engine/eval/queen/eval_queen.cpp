#include <bit>
#include "../evaluator.hpp"
#include <algorithm>

namespace engine {

int32_t Evaluator::evalEarlyQueen(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_QUEEN_START = chess::Board::bitMask(59);
    static constexpr uint64_t BLACK_QUEEN_START = chess::Board::bitMask(3);
    static constexpr int32_t EARLY_QUEEN_DEV_PENALTY = 20;

    int32_t score = 0;

    score -= (b.queens_bb[0] && !(b.queens_bb[0] & WHITE_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;
    score += (b.queens_bb[1] && !(b.queens_bb[1] & BLACK_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;

    return score;
}

inline int32_t Evaluator::evalQueenEndgamePressureSide(const chess::Board& b, int side, int ourQueens, int oppQueens) noexcept {
    if (ourQueens == 0) return 0;

    const int oppSide = side ^ 1;

    const int oppPawns = std::popcount(b.pawns_bb[oppSide]);
    const int oppKnights = std::popcount(b.knights_bb[oppSide]);
    const int oppBishops = std::popcount(b.bishops_bb[oppSide]);
    const int oppRooks = std::popcount(b.rooks_bb[oppSide]);

    const int oppMaterial = oppQueens * 900 + oppRooks * 500 +
                            oppBishops * 330 + oppKnights * 320 + oppPawns * 100;
    // Activate for any clearly losing position (up to ~Q+minor vs Q situations).
    if (oppMaterial > 1300) return 0;

    const uint64_t enemyKingBB = b.kings_bb[oppSide];
    if (!enemyKingBB) return 0;

    const int enemyKingSq = std::countr_zero(enemyKingBB);

    constexpr int32_t QUEEN_EG_EDGE_BONUS = 55;
    int32_t sideScore = edgeProximity(enemyKingSq) * QUEEN_EG_EDGE_BONUS;
    sideScore += ownKingProximity(b.kings_bb[side], enemyKingSq) * 14;

    const uint64_t queenBB = b.queens_bb[side];
    if (queenBB) {
        uint64_t tempQueens = queenBB;
        int bestQueenDist = 100;
        while (tempQueens) {
            bestQueenDist = std::min(bestQueenDist, manhattan(popLSB(tempQueens), enemyKingSq));
        }

        if (bestQueenDist >= 2 && bestQueenDist <= 5) {
            sideScore += 24;
        } else if (bestQueenDist <= 7) {
            sideScore += 10;
        }
    }

    constexpr int32_t QUEEN_EG_PRESSURE_CAP = 180;
    sideScore = std::min(sideScore, QUEEN_EG_PRESSURE_CAP);

    const int sign = (side == 0) ? 1 : -1;
    return sign * sideScore;
}

int32_t Evaluator::evalQueenEndgamePressure(const chess::Board& b) noexcept {
    const int whiteQueens = std::popcount(b.queens_bb[0]);
    const int blackQueens = std::popcount(b.queens_bb[1]);

    if (whiteQueens == 0 && blackQueens == 0) return 0;

    return evalQueenEndgamePressureSide(b, 0, whiteQueens, blackQueens)
         + evalQueenEndgamePressureSide(b, 1, blackQueens, whiteQueens);
}

} // namespace engine
