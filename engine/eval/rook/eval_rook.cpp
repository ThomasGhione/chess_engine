#include <bit>
#include "../evaluator.hpp"
#include <algorithm>

namespace engine {

inline int32_t Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
    int32_t score = 0;

    const int sign = (color == 0) ? 1 : -1;
    const bool isWhite = (color == 0);
    const int targetRank = (color == 0) ? 1 : 6; // rank index of each side's 7th rank

    while (rooks) {
        const int sq = popLSB(rooks);
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

        const uint64_t rankAboveMask = RANK_ABOVE_MASKS[rank];
        const uint64_t rankBelowMask = RANK_BELOW_MASKS[rank];

        const uint64_t ownPasserCandidates = ownFilePawns & (isWhite ? rankBelowMask : rankAboveMask);
        if (ownPasserCandidates) {
            uint64_t pawnsLoop = ownPasserCandidates;
            const uint64_t enemyAdjAndFile = oppPawns & ADJACENT_AND_FILE_MASKS[file];
            do {
                const int pawnSq = std::countr_zero(pawnsLoop);
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
                const int pawnSq = std::countr_zero(pawnsLoop);
                const uint64_t forwardFill = isWhite ? BLACK_FORWARD_FILL[pawnSq] : WHITE_FORWARD_FILL[pawnSq];
                if ((ourAdjAndFile & forwardFill) == 0ULL) {
                    score += sign * engine::ROOK_BEHIND_ENEMY_PASSER_BONUS;
                    break;
                }
                pawnsLoop &= pawnsLoop - 1;
            } while (pawnsLoop);
        }
        
    }

    return score;
}

inline int32_t Evaluator::evalRookEndgamePressureSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept {
    const bool sideHasAdvantage = (side == 0) ? (whiteRooks > blackRooks) : (blackRooks > whiteRooks);
    if (!sideHasAdvantage) return 0;

    const int oppSide = side ^ 1;
    const int oppQueens = std::popcount(b.queens_bb[oppSide]);
    const int oppBishops = std::popcount(b.bishops_bb[oppSide]);
    const int oppKnights = std::popcount(b.knights_bb[oppSide]);
    const int oppRooks2 = (side == 0) ? blackRooks : whiteRooks;
    const int oppMaterial = oppQueens * 900 + oppRooks2 * 500 + oppBishops * 330 + oppKnights * 320;

    if (oppMaterial > 400) return 0;

    const int sign = (side == 0) ? 1 : -1;
    const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
    if (!enemyKingBB) return 0;

    const int enemyKingSq = std::countr_zero(enemyKingBB);

    const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
    if (ourRooks >= 2) return 0;

    int32_t score = sign * edgeProximity(enemyKingSq) * engine::ROOK_EG_EDGE_BONUS;
    score += sign * ownKingProximity(b.kings_bb[side], enemyKingSq) * engine::ROOK_EG_PRESSURE_BONUS / 14;

    return score;
}

inline int32_t Evaluator::evalDoubleRookEndgameSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept {
    const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
    const int oppRooks = (side == 0) ? blackRooks : whiteRooks;

    if (ourRooks < 2 || ourRooks <= oppRooks) return 0;

    const int oppSide = side ^ 1;
    const int oppQueens = std::popcount(b.queens_bb[oppSide]);
    const int oppBishops = std::popcount(b.bishops_bb[oppSide]);
    const int oppKnights = std::popcount(b.knights_bb[oppSide]);
    const int oppMaterial = oppQueens * 900 + oppRooks * 500 + oppBishops * 330 + oppKnights * 320;

    if (oppMaterial > 500) return 0;

    const int sign = (side == 0) ? 1 : -1;
    const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
    if (!enemyKingBB) return 0;

    const int enemyKingSq = std::countr_zero(enemyKingBB);
    const int rank = chess::Board::rank(enemyKingSq);
    const int file = chess::Board::file(enemyKingSq);

    constexpr int32_t DOUBLE_ROOK_EDGE_BONUS = 55;
    int32_t score = sign * edgeProximity(enemyKingSq) * DOUBLE_ROOK_EDGE_BONUS;

    uint64_t rooksBB = b.rooks_bb[side];
    if ((rooksBB & (rooksBB - 1)) != 0ULL) {
        const int rook1 = popLSB(rooksBB);
        const int rook2 = std::countr_zero(rooksBB);

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

    score += sign * ownKingProximity(b.kings_bb[side], enemyKingSq) * 4;

    return score;
}

int32_t Evaluator::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return evalRooksForColor(0, whiteRooks, whitePawns, blackPawns)
         + evalRooksForColor(1, blackRooks, blackPawns, whitePawns);
}

int32_t Evaluator::evalRookEndgamePressure(const chess::Board& b) noexcept {
    const int whiteRooks = std::popcount(b.rooks_bb[0]);
    const int blackRooks = std::popcount(b.rooks_bb[1]);

    if (whiteRooks == blackRooks) return 0;

    return evalRookEndgamePressureSide(b, 0, whiteRooks, blackRooks)
         + evalRookEndgamePressureSide(b, 1, whiteRooks, blackRooks);
}

int32_t Evaluator::evalDoubleRookEndgame(const chess::Board& b) noexcept {
    const int whiteRooks = std::popcount(b.rooks_bb[0]);
    const int blackRooks = std::popcount(b.rooks_bb[1]);

    return evalDoubleRookEndgameSide(b, 0, whiteRooks, blackRooks)
         + evalDoubleRookEndgameSide(b, 1, whiteRooks, blackRooks);
}

} // namespace engine
