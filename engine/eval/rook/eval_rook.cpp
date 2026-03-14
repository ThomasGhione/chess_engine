#include "../evaluator.hpp"
#include <algorithm>

namespace engine {

inline bool Evaluator::rookIsBehindPasser(int rookRank, uint64_t filePawns, uint64_t oppPawns, bool isWhite, bool checkOwnPasser) noexcept {
    uint64_t pawns = filePawns;
    while (pawns) {
        const int pawnSq = popLSB(pawns);
        const int file = chess::Board::fileOf(static_cast<uint8_t>(pawnSq));
        
        const bool isPassed = checkOwnPasser
            ? (isWhite ? isWhitePassedPawn(pawnSq, file, oppPawns) : isBlackPassedPawn(pawnSq, file, oppPawns))
            : (isWhite ? isBlackPassedPawn(pawnSq, file, oppPawns) : isWhitePassedPawn(pawnSq, file, oppPawns));
        
        if (!isPassed) continue;
        
        const int pawnRank = chess::Board::rankOf(static_cast<uint8_t>(pawnSq));
        const bool isBehind = checkOwnPasser
            ? (isWhite ? (rookRank > pawnRank) : (rookRank < pawnRank))
            : (isWhite ? (rookRank < pawnRank) : (rookRank > pawnRank));
        
        if (isBehind) return true;
    }
    return false;
}

inline int32_t Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
    int32_t score = 0;

    const int sign = (color == 0) ? 1 : -1;
    const bool isWhite = (color == 0);
    const int targetRank = (color == 0) ? 6 : 1;

    while (rooks) {
        const int sq = popLSB(rooks);
        const int file = chess::Board::fileOf(static_cast<uint8_t>(sq));
        const int rank = chess::Board::rankOf(static_cast<uint8_t>(sq));
        const uint64_t fm = FILE_MASKS[file];
        const bool ownPawnOnFile = (ownPawns & fm) != 0;
        const bool oppPawnOnFile = (oppPawns & fm) != 0;
        const int32_t fileBonus = (!ownPawnOnFile) * ((!oppPawnOnFile) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS) * sign;
        score += fileBonus + (rank == targetRank) * (sign * engine::ROOK_ON_SEVENTH_BONUS);

        if (rookIsBehindPasser(rank, ownPawns & fm, oppPawns, isWhite, true)) {
            score += sign * engine::ROOK_BEHIND_OWN_PASSER_BONUS;
        }

        if (rookIsBehindPasser(rank, oppPawns & fm, ownPawns, isWhite, false)) {
            score += sign * engine::ROOK_BEHIND_ENEMY_PASSER_BONUS;
        }
    }

    return score;
}

inline int32_t Evaluator::evalRookEndgamePressureSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept {
    const bool sideHasAdvantage = (side == 0) ? (whiteRooks > blackRooks) : (blackRooks > whiteRooks);
    if (!sideHasAdvantage) return 0;

    const int oppSide = side ^ 1;
    const int oppQueens = __builtin_popcountll(b.queens_bb[oppSide]);
    const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
    const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
    const int oppRooks2 = (side == 0) ? blackRooks : whiteRooks;
    const int oppMaterial = oppQueens * 900 + oppRooks2 * 500 + oppBishops * 330 + oppKnights * 320;

    if (oppMaterial > 400) return 0;

    const int sign = (side == 0) ? 1 : -1;
    const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
    if (!enemyKingBB) return 0;

    const int enemyKingSq = __builtin_ctzll(enemyKingBB);
    const int rank = chess::Board::rankOf(enemyKingSq);
    const int file = chess::Board::fileOf(enemyKingSq);

    const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
    const int edgeProximity = 7 - distToEdge;

    const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
    if (ourRooks >= 2) return 0;

    int32_t score = sign * edgeProximity * engine::ROOK_EG_EDGE_BONUS;

    const uint64_t ourKingBB = b.kings_bb[side];
    if (ourKingBB) {
        const int ourKingSq = __builtin_ctzll(ourKingBB);
        const int kingDist = manhattan(ourKingSq, enemyKingSq);

        const int proximityBonus = std::max(0, 14 - kingDist);
        score += sign * proximityBonus * engine::ROOK_EG_PRESSURE_BONUS / 14;
    }

    return score;
}

inline int32_t Evaluator::evalDoubleRookEndgameSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept {
    const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
    const int oppRooks = (side == 0) ? blackRooks : whiteRooks;

    if (ourRooks < 2 || ourRooks <= oppRooks) return 0;

    const int oppSide = side ^ 1;
    const int oppQueens = __builtin_popcountll(b.queens_bb[oppSide]);
    const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
    const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
    const int oppMaterial = oppQueens * 900 + oppRooks * 500 + oppBishops * 330 + oppKnights * 320;

    if (oppMaterial > 500) return 0;

    const int sign = (side == 0) ? 1 : -1;
    const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
    if (!enemyKingBB) return 0;

    const int enemyKingSq = __builtin_ctzll(enemyKingBB);
    const int rank = chess::Board::rankOf(enemyKingSq);
    const int file = chess::Board::fileOf(enemyKingSq);

    const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
    const int edgeProximity = 7 - distToEdge;

    constexpr int32_t DOUBLE_ROOK_EDGE_BONUS = 55;
    int32_t score = sign * edgeProximity * DOUBLE_ROOK_EDGE_BONUS;

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

    score += evalRookEndgamePressureSide(b, 0, whiteRooks, blackRooks);
    score += evalRookEndgamePressureSide(b, 1, whiteRooks, blackRooks);

    return score;
}

int32_t Evaluator::evalDoubleRookEndgame(const chess::Board& b) noexcept {
    int32_t score = 0;

    const int whiteRooks = __builtin_popcountll(b.rooks_bb[0]);
    const int blackRooks = __builtin_popcountll(b.rooks_bb[1]);

    score += evalDoubleRookEndgameSide(b, 0, whiteRooks, blackRooks);
    score += evalDoubleRookEndgameSide(b, 1, whiteRooks, blackRooks);

    return score;
}

} // namespace engine
