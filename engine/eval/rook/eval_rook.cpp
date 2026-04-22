#include "../evaluator.hpp"
#include <algorithm>

namespace engine {

inline int32_t Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
    int32_t score = 0;

    const int sign = (color == 0) ? 1 : -1;
    const bool isWhite = (color == 0);
    const int targetRank = (color == 0) ? 6 : 1;

    while (rooks) {
        const int sq = __builtin_ctzll(rooks);
        const int file = chess::Board::file(sq);
        const int rank = chess::Board::rank(sq);
        const uint64_t fm = FILE_MASKS[file];
        const uint64_t ownFilePawns = ownPawns & fm;
        const uint64_t oppFilePawns = oppPawns & fm;

        if (!ownFilePawns) {
            score += ((!oppFilePawns) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS) * sign;
        }

        if (rank == targetRank) {
            score += sign * engine::ROOK_ON_SEVENTH_BONUS;
        }

        const uint64_t rankAboveMask = rank == 7 ? 0ULL : (~0ULL << ((rank + 1) * 8));
        const uint64_t rankBelowMask = (1ULL << (rank * 8)) - 1;

        const uint64_t ownPasserCandidates = ownFilePawns & (isWhite ? rankBelowMask : rankAboveMask);
        if (ownPasserCandidates) {
            uint64_t pawnsLoop = ownPasserCandidates;
            const uint64_t enemyAdjAndFile = oppPawns & ADJACENT_AND_FILE_MASKS[file];
            do {
                const int pawnSq = __builtin_ctzll(pawnsLoop);
                const uint64_t forwardFill = isWhite ? WHITE_FORWARD_FILL[pawnSq] : BLACK_FORWARD_FILL[pawnSq];
                if ((enemyAdjAndFile & forwardFill) == 0ULL) {
                    score += sign * engine::ROOK_BEHIND_OWN_PASSER_BONUS;
                    break;
                }
                pawnsLoop &= pawnsLoop - 1;
            } while (pawnsLoop);
        }

        const uint64_t enemyPasserCandidates = oppFilePawns & (isWhite ? rankAboveMask : rankBelowMask);
        if (enemyPasserCandidates) {
            uint64_t pawnsLoop = enemyPasserCandidates;
            const uint64_t ourAdjAndFile = ownPawns & ADJACENT_AND_FILE_MASKS[file];
            do {
                const int pawnSq = __builtin_ctzll(pawnsLoop);
                const uint64_t forwardFill = isWhite ? BLACK_FORWARD_FILL[pawnSq] : WHITE_FORWARD_FILL[pawnSq];
                if ((ourAdjAndFile & forwardFill) == 0ULL) {
                    score += sign * engine::ROOK_BEHIND_ENEMY_PASSER_BONUS;
                    break;
                }
                pawnsLoop &= pawnsLoop - 1;
            } while (pawnsLoop);
        }
        
        rooks &= rooks - 1;
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
    const int rank = chess::Board::rank(enemyKingSq);
    const int file = chess::Board::file(enemyKingSq);

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
    const int rank = chess::Board::rank(enemyKingSq);
    const int file = chess::Board::file(enemyKingSq);

    const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
    const int edgeProximity = 7 - distToEdge;

    constexpr int32_t DOUBLE_ROOK_EDGE_BONUS = 55;
    int32_t score = sign * edgeProximity * DOUBLE_ROOK_EDGE_BONUS;

    uint64_t rooksBB = b.rooks_bb[side];
    if (((rooksBB & (rooksBB - 1)) != 0ULL)) {
        const int rook1 = __builtin_ctzll(rooksBB);
        rooksBB &= (rooksBB - 1);
        const int rook2 = __builtin_ctzll(rooksBB);

        const int r1_rank = chess::Board::rank(rook1);
        const int r1_file = chess::Board::file(rook1);
        const int r2_rank = chess::Board::rank(rook2);
        const int r2_file = chess::Board::file(rook2);

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
