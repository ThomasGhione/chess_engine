#include "../evaluator.hpp"

namespace engine {

// Opposite-color bishop endgame scaling.
// When each side has exactly one bishop on opposite colors and little other
// material, the position is drawish even with a pawn advantage. Scale the
// result toward zero; the more pawns the winning side has, the less we scale.
int32_t Evaluator::applyOppColorBishopScaling(const chess::Board& b, int32_t score) noexcept {
    if (score == 0) return 0;

    const uint64_t wb = b.bishops_bb[0];
    const uint64_t bb = b.bishops_bb[1];
    if (__builtin_popcountll(wb) != 1 || __builtin_popcountll(bb) != 1) return score;

    const bool whiteOnDark = (wb & DARK_SQUARES) != 0ULL;
    const bool blackOnDark = (bb & DARK_SQUARES) != 0ULL;
    if (whiteOnDark == blackOnDark) return score;

    const int wMajors = __builtin_popcountll(b.queens_bb[0])  * 900
                      + __builtin_popcountll(b.rooks_bb[0])   * 500
                      + __builtin_popcountll(b.knights_bb[0]) * 320;
    const int bMajors = __builtin_popcountll(b.queens_bb[1])  * 900
                      + __builtin_popcountll(b.rooks_bb[1])   * 500
                      + __builtin_popcountll(b.knights_bb[1]) * 320;

    if (std::abs(wMajors - bMajors) > 400) return score;

    const int pawnImbalance = std::abs(
        static_cast<int>(__builtin_popcountll(b.pawns_bb[0])) -
        static_cast<int>(__builtin_popcountll(b.pawns_bb[1]))
    );

    // Scale factor out of 64: base 32 (half eval), +8 per extra pawn, cap 64.
    constexpr int SCALE_BASE     = 32;
    constexpr int SCALE_PER_PAWN =  8;
    constexpr int SCALE_MAX      = 64;
    const int scaleFactor = std::min(SCALE_MAX, SCALE_BASE + pawnImbalance * SCALE_PER_PAWN);

    return (score * scaleFactor) / SCALE_MAX;
}

template<int Side>
inline constexpr int32_t Evaluator::evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
    static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");

    const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
    const int lightPawnCount = __builtin_popcountll(pawns & LIGHT_SQUARES);

    const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
    const int lightBishops = __builtin_popcountll(bishops & LIGHT_SQUARES);

    const int32_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * BAD_BISHOP_PAWN_MULTIPLIER);

    if constexpr (Side == 0) {
        return score;
    } else {
        return -score;
    }
}

int32_t Evaluator::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return (side == 0) ? Evaluator::evalBadBishopImpl<0>(bishops, pawns) : Evaluator::evalBadBishopImpl<1>(bishops, pawns);
}

inline int32_t Evaluator::evalCentralBlockPenalty(uint8_t blockerType, int fullMoves) noexcept {
    int penalty;
    switch (blockerType) {
        case chess::Board::BISHOP: penalty = BLOCK_PENALTY_BISHOP; break;
        case chess::Board::KNIGHT: penalty = BLOCK_PENALTY_KNIGHT; break;
        case chess::Board::QUEEN:  penalty = BLOCK_PENALTY_QUEEN; break;
        case chess::Board::ROOK:   penalty = BLOCK_PENALTY_ROOK; break;
        default: penalty = BLOCK_PENALTY_DEFAULT; break;
    }

    if (fullMoves <= BLOCK_MIDGAME_EARLY_THRESHOLD) {
        penalty += BLOCK_OPENING_BONUS;
    } else if (fullMoves <= BLOCK_MIDGAME_THRESHOLD) {
        penalty += BLOCK_MIDGAME_BONUS;
    }

    return penalty;
}

inline int32_t Evaluator::evalBlockedPawnByBishopsPawn(const chess::Board& b, int side, uint64_t bishops, int fullMoves, int psq) noexcept {
    const int rank = chess::Board::rank(psq);
    const int file = chess::Board::file(psq);
    const bool pawnOnStart = rank == (side == 0 ? 6 : 1);
    const int forward = side == 0 ? (psq - 8) : (psq + 8);
    if (forward < 0 || forward >= 64) return 0;

    int32_t penalty = 0;
    const bool centralStart = pawnOnStart && (file == 3 || file == 4);
    const uint8_t blocker = b.get(forward);
    const uint8_t ownColor = (side == 0) ? chess::Board::WHITE : chess::Board::BLACK;

    if (centralStart && blocker != chess::Board::EMPTY && (blocker & chess::Board::MASK_COLOR) == ownColor) {
        penalty += evalCentralBlockPenalty(blocker & chess::Board::MASK_PIECE_TYPE, fullMoves);
    }

    if (bishops & chess::Board::bitMask(forward)) {
        int blockPenalty = BLOCK_PAWN_BISHOP_PENALTY;
        if (file == 3 || file == 4) {
            blockPenalty += BLOCK_PAWN_CENTER_FILE_BONUS;
        }
        if (pawnOnStart) blockPenalty += BLOCK_PAWN_START_BONUS;
        penalty += blockPenalty;
    }

    const int sign = (side == 0) ? 1 : -1;
    return sign * (-penalty);
}

inline int32_t Evaluator::evalBlockedPawnByBishopsSide(const chess::Board& b, int side, int fullMoves) noexcept {
    const uint64_t pawns = b.pawns_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    if (!pawns || !bishops) return 0;

    int32_t score = 0;
    uint64_t pawnsCopy = pawns;
    while (pawnsCopy) {
        const int psq = popLSB(pawnsCopy);
        score += evalBlockedPawnByBishopsPawn(b, side, bishops, fullMoves, psq);
    }

    return score;
}

int32_t Evaluator::evalBlockedPawnByBishops(const chess::Board& b) noexcept {
    const int fullMoves = b.getFullMoveClock();

    const int32_t evalBlack = evalBlockedPawnByBishopsSide(b, 0, fullMoves);
    const int32_t evalWhite = evalBlockedPawnByBishopsSide(b, 1, fullMoves);

    return evalBlack + evalWhite;
}

} // namespace engine
