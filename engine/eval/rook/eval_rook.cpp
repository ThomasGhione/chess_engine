#include <bit>
#include "../evaluator.hpp"
#include <algorithm>

namespace engine {

namespace {
// Rough non-pawn material (centipawns) for `side`, used by the rook-endgame
// heuristics to detect a near-lone enemy king. Values are intentionally fixed
// (independent of the tunable PIECE_VALUES) so the thresholds stay stable.
inline int roughNonPawnMaterial(const chess::Board& b, int side, int rookCount) noexcept {
    return std::popcount(b.queens_bb[side]) * 900 + rookCount * 500
         + std::popcount(b.bishops_bb[side]) * 330 + std::popcount(b.knights_bb[side]) * 320;
}
} // namespace

inline PhaseValue Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
    PhaseValue score{};

    const int sign = (color == 0) ? 1 : -1;
    const bool isWhite = (color == 0);
    const int targetRank = (color == 0) ? 1 : 6;

    while (rooks) {
        const int sq = popLSB(rooks);
        const int file = chess::Board::file(sq);
        const int rank = chess::Board::rank(sq);
        const uint64_t fm = FILE_MASKS[file];
        const uint64_t ownFilePawns = ownPawns & fm;
        const uint64_t oppFilePawns = oppPawns & fm;

        if (!ownFilePawns) {
            score += sign * ((!oppFilePawns) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS);
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

inline PhaseValue Evaluator::evalRookEndgamePressureSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept {
    const bool sideHasAdvantage = (side == 0) ? (whiteRooks > blackRooks) : (blackRooks > whiteRooks);
    if (!sideHasAdvantage) return {};

    const int oppSide = side ^ 1;
    const int oppRooks = (side == 0) ? blackRooks : whiteRooks;
    if (roughNonPawnMaterial(b, oppSide, oppRooks) > 400) return {};

    const int sign = (side == 0) ? 1 : -1;
    const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
    if (!enemyKingBB) return {};

    const int enemyKingSq = std::countr_zero(enemyKingBB);

    const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
    if (ourRooks >= 2) return {};

    PhaseValue score = (sign * edgeProximity(enemyKingSq)) * engine::ROOK_EG_EDGE_BONUS;
    // Divide by 14 applied per-side (PhaseValue / int32 not defined → split).
    const int32_t proxScale = sign * ownKingProximity(b.kings_bb[side], enemyKingSq);
    score.mg += (proxScale * engine::ROOK_EG_PRESSURE_BONUS.mg) / 14;
    score.eg += (proxScale * engine::ROOK_EG_PRESSURE_BONUS.eg) / 14;

    return score;
}

inline PhaseValue Evaluator::evalDoubleRookEndgameSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept {
    const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
    const int oppRooks = (side == 0) ? blackRooks : whiteRooks;

    if (ourRooks < 2 || ourRooks <= oppRooks) return {};

    const int oppSide = side ^ 1;
    if (roughNonPawnMaterial(b, oppSide, oppRooks) > 500) return {};

    const int sign = (side == 0) ? 1 : -1;
    const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
    if (!enemyKingBB) return {};

    const int enemyKingSq = std::countr_zero(enemyKingBB);
    const int rank = chess::Board::rank(enemyKingSq);
    const int file = chess::Board::file(enemyKingSq);

    // Local constants (kept scalar; eg-side bonus magnitudes baked into raw nums).
    constexpr int32_t DOUBLE_ROOK_EDGE_BONUS    = 55;
    constexpr int32_t DOUBLE_ROOK_RANKFILE_BONUS = 28;
    constexpr int32_t DOUBLE_ROOK_ON_KING_LINE  = 22;
    constexpr int32_t DOUBLE_ROOK_PROX_SCALE    = 4;

    // These are pure-endgame contributions (mg=0).
    PhaseValue score{0, sign * edgeProximity(enemyKingSq) * DOUBLE_ROOK_EDGE_BONUS};

    uint64_t rooksBB = b.rooks_bb[side];
    const int rook1 = popLSB(rooksBB);
    const int rook2 = std::countr_zero(rooksBB);

    const int r1_rank = chess::Board::rank(rook1);
    const int r1_file = chess::Board::file(rook1);
    const int r2_rank = chess::Board::rank(rook2);
    const int r2_file = chess::Board::file(rook2);

    if (r1_rank == r2_rank || r1_file == r2_file) {
        score.eg += sign * DOUBLE_ROOK_RANKFILE_BONUS;
    }

    if (r1_rank == rank || r2_rank == rank || r1_file == file || r2_file == file) {
        score.eg += sign * DOUBLE_ROOK_ON_KING_LINE;
    }

    score.eg += sign * ownKingProximity(b.kings_bb[side], enemyKingSq) * DOUBLE_ROOK_PROX_SCALE;

    return score;
}

PhaseValue Evaluator::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return evalRooksForColor(0, whiteRooks, whitePawns, blackPawns)
         + evalRooksForColor(1, blackRooks, blackPawns, whitePawns);
}

PhaseValue Evaluator::evalRookEndgamePressure(const chess::Board& b) noexcept {
    const int whiteRooks = std::popcount(b.rooks_bb[0]);
    const int blackRooks = std::popcount(b.rooks_bb[1]);

    if (whiteRooks == blackRooks) return {};

    return evalRookEndgamePressureSide(b, 0, whiteRooks, blackRooks)
         + evalRookEndgamePressureSide(b, 1, whiteRooks, blackRooks);
}

PhaseValue Evaluator::evalDoubleRookEndgame(const chess::Board& b) noexcept {
    const int whiteRooks = std::popcount(b.rooks_bb[0]);
    const int blackRooks = std::popcount(b.rooks_bb[1]);

    return evalDoubleRookEndgameSide(b, 0, whiteRooks, blackRooks)
         + evalDoubleRookEndgameSide(b, 1, whiteRooks, blackRooks);
}

} // namespace engine
