#include "evaluator.hpp"

namespace engine {

template<int Side>
inline constexpr int64_t Evaluator::evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
    static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");

    const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
    const int lightPawnCount = __builtin_popcountll(pawns & LIGHT_SQUARES);

    const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
    const int lightBishops = __builtin_popcountll(bishops & LIGHT_SQUARES);

    const int64_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * 8);

    if constexpr (Side == 0) {
        return score;
    } else {
        return -score;
    }
}

int64_t Evaluator::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return (side == 0) ? Evaluator::evalBadBishopImpl<0>(bishops, pawns) : Evaluator::evalBadBishopImpl<1>(bishops, pawns);
}

int64_t Evaluator::evalBlockedPawnByBishops(const chess::Board& b) noexcept {
    int64_t score = 0;
    const int fullMoves = b.getFullMoveClock();

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        uint64_t pawns = b.pawns_bb[side];
        const uint64_t bishops = b.bishops_bb[side];

        if (!pawns || !bishops) continue;

        while (pawns) {
            const int psq = popLSB(pawns);
            const int rank = chess::Board::rankOf(psq);
            const int file = chess::Board::fileOf(psq);
            const int startRank = (side == 0) ? 6 : 1;
            const bool pawnOnStart = (rank == startRank);

            const int forward = (side == 0) ? (psq - 8) : (psq + 8);
            if (forward < 0 || forward >= 64) continue;

            // Strong opening penalty when a friendly piece blocks d/e pawn from its start square.
            if (pawnOnStart && (file == 3 || file == 4)) {
                const uint8_t forwardSq = static_cast<uint8_t>(forward);
                const uint8_t blocker = b.get(forwardSq);
                const uint8_t ownColor = (side == 0) ? chess::Board::WHITE : chess::Board::BLACK;
                if (blocker != chess::Board::EMPTY && (blocker & chess::Board::MASK_COLOR) == ownColor) {
                    const uint8_t blockerType = blocker & chess::Board::MASK_PIECE_TYPE;
                    int centralBlockPenalty = 0;
                    switch (blockerType) {
                        case chess::Board::BISHOP: centralBlockPenalty = 34; break;
                        case chess::Board::KNIGHT: centralBlockPenalty = 28; break;
                        case chess::Board::QUEEN:  centralBlockPenalty = 24; break;
                        case chess::Board::ROOK:   centralBlockPenalty = 18; break;
                        default: centralBlockPenalty = 12; break;
                    }
                    if (fullMoves <= 10) {
                        centralBlockPenalty += 10;
                    } else if (fullMoves <= 16) {
                        centralBlockPenalty += 5;
                    }
                    score += sign * (-centralBlockPenalty);
                }
            }

            if (bishops & chess::Board::bitMask(forward)) {
                // Keep this modest: over-penalizing can force artificial bishop retreats.
                int penaltyVal = 10;
                if (file == 3 || file == 4) penaltyVal += 8;
                if (pawnOnStart) penaltyVal += 6;

                score += sign * (-penaltyVal);
            }
        }
    }

    return score;
}

} // namespace engine
