#include "evaluator.hpp"
#include <algorithm>

namespace engine {

int64_t Evaluator::evalEarlyQueen(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_QUEEN_START = chess::Board::bitMask(59);
    static constexpr uint64_t BLACK_QUEEN_START = chess::Board::bitMask(3);
    static constexpr int64_t EARLY_QUEEN_DEV_PENALTY = 20;

    int64_t score = 0;

    score -= (b.queens_bb[0] && !(b.queens_bb[0] & WHITE_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;
    score += (b.queens_bb[1] && !(b.queens_bb[1] & BLACK_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;

    return score;
}

int64_t Evaluator::evalQueenEndgamePressure(const chess::Board& b) noexcept {
    int64_t score = 0;

    const int whiteQueens = __builtin_popcountll(b.queens_bb[0]);
    const int blackQueens = __builtin_popcountll(b.queens_bb[1]);

    if (whiteQueens == 0 && blackQueens == 0) {
        return 0;
    }

    for (int side = 0; side < 2; ++side) {
        const int ourQueens = (side == 0) ? whiteQueens : blackQueens;
        if (ourQueens == 0) continue;

        const int oppSide = side ^ 1;

        const int oppPawns = __builtin_popcountll(b.pawns_bb[oppSide]);
        const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
        const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
        const int oppRooks = __builtin_popcountll(b.rooks_bb[oppSide]);
        const int oppQueens = (side == 0) ? blackQueens : whiteQueens;

        const int oppMaterial = oppQueens * 900 + oppRooks * 500 +
                                oppBishops * 330 + oppKnights * 320 + oppPawns * 100;
        if (oppMaterial > 700) continue;

        const int sign = (side == 0) ? 1 : -1;
        const uint64_t enemyKingBB = b.kings_bb[oppSide];
        if (!enemyKingBB) continue;

        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const int rank = chess::Board::rankOf(enemyKingSq);
        const int file = chess::Board::fileOf(enemyKingSq);

        const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
        const int edgeProximity = 7 - distToEdge;

        // REDUCED: was 120 -> edgeProximity 7 (corner) × 120 = 840cp ≈ a queen!
        // The engine would sacrifice everything to keep its queen thinking it
        // had a won endgame.  35cp × 7 = 245cp max — substantial advantage
        // but never exceeds a minor piece on its own.
        constexpr int64_t QUEEN_EG_EDGE_BONUS = 35;
        int64_t sideScore = edgeProximity * QUEEN_EG_EDGE_BONUS;

        const uint64_t ourKingBB = b.kings_bb[side];
        if (ourKingBB) {
            const int ourKingSq = __builtin_ctzll(ourKingBB);
            const int kingDist = manhattan(ourKingSq, enemyKingSq);

            const int proximityBonus = std::max(0, 14 - kingDist);
            sideScore += proximityBonus * 20;
        }

        const uint64_t queenBB = b.queens_bb[side];
        if (queenBB) {
            uint64_t tempQueens = queenBB;
            int bestQueenDist = 100;
            while (tempQueens) {
                 const int qSq = __builtin_ctzll(tempQueens);
                 tempQueens &= tempQueens - 1;
                 bestQueenDist = std::min(bestQueenDist, manhattan(qSq, enemyKingSq));
            }

            if (bestQueenDist >= 2 && bestQueenDist <= 5) {
                sideScore += 40;
            } else if (bestQueenDist <= 7) {
                sideScore += 15;
            }
        }

        // CAP: queen endgame pressure should never exceed ~2.5 pawns.
        // Without this cap the combined terms could reach >1000cp, causing
        // the engine to sacrifice pieces to reach a "queen vs lone king" ending.
        constexpr int64_t QUEEN_EG_PRESSURE_CAP = 250;
        sideScore = std::min(sideScore, QUEEN_EG_PRESSURE_CAP);
        score += sign * sideScore;
    }

    return score;
}

} // namespace engine
