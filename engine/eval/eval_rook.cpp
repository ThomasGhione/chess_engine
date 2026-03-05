#include "evaluator.hpp"
#include <algorithm>

namespace engine {

inline int32_t Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
    int32_t score = 0;

    const int sign = (color == 0) ? 1 : -1;
    const bool isWhite = (color == 0);
    const int targetRank = (color == 0) ? 6 : 1;

    while (rooks) {
        const int sq = popLSB(rooks);
        const int file = sq & 7;
        const int rank = sq >> 3;
        const uint64_t fm = FILE_MASKS[file];
        const bool ownPawnOnFile = (ownPawns & fm) != 0;
        const bool oppPawnOnFile = (oppPawns & fm) != 0;
        const int32_t fileBonus = (!ownPawnOnFile) * ((!oppPawnOnFile) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS) * sign;
        score += fileBonus + (rank == targetRank) * (sign * engine::ROOK_ON_SEVENTH_BONUS);

        bool rookBehindOwnPasser = false;
        uint64_t ownFilePawns = ownPawns & fm;
        while (ownFilePawns) {
            const int pawnSq = popLSB(ownFilePawns);
            const bool ownPassed = isWhite
                ? isWhitePassedPawn(pawnSq, file, oppPawns)
                : isBlackPassedPawn(pawnSq, file, oppPawns);
            if (!ownPassed) continue;
            const int pawnRank = pawnSq >> 3;
            const bool isBehindOwn = isWhite ? (rank > pawnRank) : (rank < pawnRank);
            if (isBehindOwn) {
                rookBehindOwnPasser = true;
                break;
            }
        }
        if (rookBehindOwnPasser) {
            score += sign * engine::ROOK_BEHIND_OWN_PASSER_BONUS;
        }

        bool rookBehindEnemyPasser = false;
        uint64_t enemyFilePawns = oppPawns & fm;
        while (enemyFilePawns) {
            const int pawnSq = popLSB(enemyFilePawns);
            const bool enemyPassed = isWhite
                ? isBlackPassedPawn(pawnSq, file, ownPawns)
                : isWhitePassedPawn(pawnSq, file, ownPawns);
            if (!enemyPassed) continue;
            const int pawnRank = pawnSq >> 3;
            const bool isBehindEnemy = isWhite ? (rank < pawnRank) : (rank > pawnRank);
            if (isBehindEnemy) {
                rookBehindEnemyPasser = true;
                break;
            }
        }
        if (rookBehindEnemyPasser) {
            score += sign * engine::ROOK_BEHIND_ENEMY_PASSER_BONUS;
        }
    }

    return score;
}

int32_t Evaluator::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return evalRooksForColor(0, whiteRooks, whitePawns, blackPawns)
         + evalRooksForColor(1, blackRooks, blackPawns, whitePawns);
}

int32_t Evaluator::evalRookEndgamePressure(const chess::Board& b) noexcept {
    int32_t score = 0;

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

        const int32_t edgeBonus = engine::ROOK_EG_EDGE_BONUS;
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

int32_t Evaluator::evalDoubleRookEndgame(const chess::Board& b) noexcept {
    int32_t score = 0;

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

        constexpr int32_t DOUBLE_ROOK_EDGE_BONUS = 55;
        score += sign * edgeProximity * DOUBLE_ROOK_EDGE_BONUS;

        uint64_t rooksBB = b.rooks_bb[side];
        if (((rooksBB & (rooksBB - 1)) != 0ULL)) {
            const int rook1 = __builtin_ctzll(rooksBB);
            rooksBB &= (rooksBB - 1);
            const int rook2 = __builtin_ctzll(rooksBB);

            const int r1_rank = chess::Board::rankOf(rook1);
            const int r1_file = chess::Board::fileOf(rook1);
            const int r2_rank = chess::Board::rankOf(rook2);
            const int r2_file = chess::Board::fileOf(rook2);

            if (r1_rank == r2_rank || r1_file == r2_file) {
                score += sign * 28;
            }

            if (r1_rank == rank || r2_rank == rank || r1_file == file || r2_file == file) {
                score += sign * 22;
            }
        }

        const uint64_t ourKingBB = b.kings_bb[side];
        if (ourKingBB) {
            const int ourKingSq = __builtin_ctzll(ourKingBB);
            const int kingDist = manhattan(ourKingSq, enemyKingSq);

            const int proximityBonus = std::max(0, 14 - kingDist);
            score += sign * proximityBonus * 4;
        }
    }

    return score;
}

} // namespace engine
