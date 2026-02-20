#include "evaluator.hpp"
#include <algorithm>

namespace engine {

inline int64_t Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
    int64_t score = 0;

    const int sign = (color == 0) ? 1 : -1;
    const int targetRank = (color == 0) ? 6 : 1;

    if (!rooks) {
      return score;
    }
    const int sq = popLSB(rooks);

    const int file = sq & 7;
    const int rank = sq >> 3;
    const uint64_t fm = FILE_MASKS[file];
    const bool ownPawnOnFile = (ownPawns & fm) != 0;
    const bool oppPawnOnFile = (oppPawns & fm) != 0;
    const int64_t fileBonus = (!ownPawnOnFile) * ((!oppPawnOnFile) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS) * sign;
    score += fileBonus + (rank == targetRank) * (sign * engine::ROOK_ON_SEVENTH_BONUS);

    if (!rooks) {
      return score;
    }

    const int sq2 = popLSB(rooks);
    const int file2 = sq2 & 7;
    const int rank2 = sq2 >> 3;
    const uint64_t fm2 = FILE_MASKS[file2];
    const bool ownPawnOnFile2 = (ownPawns & fm2) != 0;
    const bool oppPawnOnFile2 = (oppPawns & fm2) != 0;
    const int64_t fileBonus2 = (!ownPawnOnFile2) * ((!oppPawnOnFile2) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS) * sign;
    score += fileBonus2 + (rank2 == targetRank) * (sign * engine::ROOK_ON_SEVENTH_BONUS);

    return score;
}

int64_t Evaluator::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const uint64_t rooksBySide[2] = {whiteRooks, blackRooks};
    const uint64_t ownPawnsBySide[2] = {whitePawns, blackPawns};
    const uint64_t oppPawnsBySide[2] = {blackPawns, whitePawns};
    int64_t score = 0;
    for (int side = 0; side < 2; ++side) {
        score += evalRooksForColor(side, rooksBySide[side], ownPawnsBySide[side], oppPawnsBySide[side]);
    }
    return score;
}

int64_t Evaluator::evalRookEndgamePressure(const chess::Board& b) noexcept {
    int64_t score = 0;

    const int whiteRooks = __builtin_popcountll(b.rooks_bb[0]);
    const int blackRooks = __builtin_popcountll(b.rooks_bb[1]);

    const bool whiteHasRookAdvantage = (whiteRooks > blackRooks);
    const bool blackHasRookAdvantage = (blackRooks > whiteRooks);

    if (!whiteHasRookAdvantage && !blackHasRookAdvantage) {
        return 0;
    }

    for (int side = 0; side < 2; ++side) {
        const bool sideHasAdvantage = (side == 0) ? whiteHasRookAdvantage : blackHasRookAdvantage;
        if (!sideHasAdvantage) continue;

        const int oppSide = side ^ 1;
        const int oppQueens = __builtin_popcountll(b.queens_bb[oppSide]);
        const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
        const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
        const int oppRooks2 = (side == 0) ? blackRooks : whiteRooks;
        const int oppMaterial = oppQueens * 900 + oppRooks2 * 500 + oppBishops * 330 + oppKnights * 320;

        if (oppMaterial > 400) continue;

        const int sign = (side == 0) ? 1 : -1;
        const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
        if (!enemyKingBB) continue;

        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const int rank = chess::Board::rankOf(enemyKingSq);
        const int file = chess::Board::fileOf(enemyKingSq);

        const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
        const int edgeProximity = 7 - distToEdge;

        const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
        if (ourRooks >= 2)
            continue;

        const int64_t edgeBonus = engine::ROOK_EG_EDGE_BONUS;
        score += sign * edgeProximity * edgeBonus;

        const uint64_t ourKingBB = b.kings_bb[side];
        if (ourKingBB) {
            const int ourKingSq = __builtin_ctzll(ourKingBB);
            const int kingDist = manhattan(ourKingSq, enemyKingSq);

            const int proximityBonus = std::max(0, 14 - kingDist);
            score += sign * proximityBonus * engine::ROOK_EG_PRESSURE_BONUS / 14;
        }
    }

    return score;
}

int64_t Evaluator::evalDoubleRookEndgame(const chess::Board& b) noexcept {
    int64_t score = 0;

    const int whiteRooks = __builtin_popcountll(b.rooks_bb[0]);
    const int blackRooks = __builtin_popcountll(b.rooks_bb[1]);

    for (int side = 0; side < 2; ++side) {
        const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
        const int oppRooks = (side == 0) ? blackRooks : whiteRooks;

        if (ourRooks < 2 || ourRooks <= oppRooks) continue;

        const int oppSide = side ^ 1;
        const int oppQueens = __builtin_popcountll(b.queens_bb[oppSide]);
        const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
        const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
        const int oppMaterial = oppQueens * 900 + oppRooks * 500 + oppBishops * 330 + oppKnights * 320;

        if (oppMaterial > 500) continue;

        const int sign = (side == 0) ? 1 : -1;
        const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
        if (!enemyKingBB) continue;

        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const int rank = chess::Board::rankOf(enemyKingSq);
        const int file = chess::Board::fileOf(enemyKingSq);

        const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
        const int edgeProximity = 7 - distToEdge;

        constexpr int64_t DOUBLE_ROOK_EDGE_BONUS = 100;
        score += sign * edgeProximity * DOUBLE_ROOK_EDGE_BONUS;

        uint64_t rooksBB = b.rooks_bb[side];
        if (__builtin_popcountll(rooksBB) >= 2) {
            const int rook1 = __builtin_ctzll(rooksBB);
            rooksBB &= (rooksBB - 1);
            const int rook2 = __builtin_ctzll(rooksBB);

            const int r1_rank = chess::Board::rankOf(rook1);
            const int r1_file = chess::Board::fileOf(rook1);
            const int r2_rank = chess::Board::rankOf(rook2);
            const int r2_file = chess::Board::fileOf(rook2);

            if (r1_rank == r2_rank || r1_file == r2_file) {
                score += sign * 50;
            }

            if (r1_rank == rank || r2_rank == rank || r1_file == file || r2_file == file) {
                score += sign * 40;
            }
        }

        const uint64_t ourKingBB = b.kings_bb[side];
        if (ourKingBB) {
            const int ourKingSq = __builtin_ctzll(ourKingBB);
            const int kingDist = manhattan(ourKingSq, enemyKingSq);

            const int proximityBonus = std::max(0, 14 - kingDist);
            score += sign * proximityBonus * 8;
        }
    }

    return score;
}

} // namespace engine
