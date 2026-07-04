#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::applyOppColorBishopScaling(const chess::Board& b, int32_t score) noexcept {
    if (score == 0) return 0;

    const uint64_t wb = b.bishops_bb[0];
    const uint64_t bb = b.bishops_bb[1];
    if (std::popcount(wb) != 1 || std::popcount(bb) != 1) return score;

    const bool whiteOnDark = (wb & DARK_SQUARES) != 0ULL;
    const bool blackOnDark = (bb & DARK_SQUARES) != 0ULL;
    if (whiteOnDark == blackOnDark) return score;

    const int wMajors = std::popcount(b.queens_bb[0])  * 900
                      + std::popcount(b.rooks_bb[0])   * 500
                      + std::popcount(b.knights_bb[0]) * 320;
    const int bMajors = std::popcount(b.queens_bb[1])  * 900
                      + std::popcount(b.rooks_bb[1])   * 500
                      + std::popcount(b.knights_bb[1]) * 320;

    if (std::abs(wMajors - bMajors) > 400) return score;

    const int pawnImbalance = std::abs(
        static_cast<int>(std::popcount(b.pawns_bb[0])) -
        static_cast<int>(std::popcount(b.pawns_bb[1]))
    );

    constexpr int SCALE_BASE     = 32;
    constexpr int SCALE_PER_PAWN =  8;
    constexpr int SCALE_MAX      = 64;
    const int scaleFactor = std::min(SCALE_MAX, SCALE_BASE + pawnImbalance * SCALE_PER_PAWN);

    return (score * scaleFactor) / SCALE_MAX;
}

template<int Side>
inline constexpr PhaseValue Evaluator::evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
    static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");

    const int darkPawnCount = std::popcount(pawns & DARK_SQUARES);
    const int lightPawnCount = std::popcount(pawns & LIGHT_SQUARES);

    const int darkBishops = std::popcount(bishops & DARK_SQUARES);
    const int lightBishops = std::popcount(bishops & LIGHT_SQUARES);

    const int32_t raw = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * BAD_BISHOP_PAWN_MULTIPLIER);

    if constexpr (Side == 0) {
        return PhaseValue{raw, raw};
    } else {
        return PhaseValue{-raw, -raw};
    }
}

PhaseValue Evaluator::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return (side == 0) ? Evaluator::evalBadBishopImpl<0>(bishops, pawns) : Evaluator::evalBadBishopImpl<1>(bishops, pawns);
}

inline PhaseValue Evaluator::evalCentralBlockPenalty(uint8_t blockerType, int fullMoves) noexcept {
    PhaseValue penalty;
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

inline PhaseValue Evaluator::evalBlockedPawnByBishopsPawn(const chess::Board& b, int side, uint64_t bishops, int fullMoves, int psq) noexcept {
    const int rank = chess::rank(psq);
    const int file = chess::file(psq);
    const bool pawnOnStart = rank == (side == 0 ? 6 : 1);
    const int forward = side == 0 ? (psq - 8) : (psq + 8);

    PhaseValue penalty{};
    const bool centralStart = pawnOnStart && (file == 3 || file == 4);
    const uint8_t blocker = b.get(forward);
    const uint8_t ownColor = (side == 0) ? chess::Board::WHITE : chess::Board::BLACK;

    if (centralStart && blocker != chess::Board::EMPTY && (blocker & chess::Board::MASK_COLOR) == ownColor) {
        penalty += evalCentralBlockPenalty(blocker & chess::Board::MASK_PIECE_TYPE, fullMoves);
    }

    if (bishops & chess::Board::BIT_MASKS[forward]) {
        PhaseValue blockPenalty = BLOCK_PAWN_BISHOP_PENALTY;
        if (file == 3 || file == 4) {
            blockPenalty += BLOCK_PAWN_CENTER_FILE_BONUS;
        }
        if (pawnOnStart) blockPenalty += BLOCK_PAWN_START_BONUS;
        penalty += blockPenalty;
    }

    const int sign = (side == 0) ? 1 : -1;
    return (-sign) * penalty;
}

inline PhaseValue Evaluator::evalBlockedPawnByBishopsSide(const chess::Board& b, int side, int fullMoves) noexcept {
    const uint64_t pawns = b.pawns_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    if (!pawns || !bishops) return {};

    PhaseValue score{};
    uint64_t pawnsCopy = pawns;
    while (pawnsCopy) {
        const int psq = popLSB(pawnsCopy);
        score += evalBlockedPawnByBishopsPawn(b, side, bishops, fullMoves, psq);
    }

    return score;
}

PhaseValue Evaluator::evalBlockedPawnByBishops(const chess::Board& b) noexcept {
    const int fullMoves = b.getFullMoveClock();
    return evalBlockedPawnByBishopsSide(b, 0, fullMoves) + evalBlockedPawnByBishopsSide(b, 1, fullMoves);
}

} // namespace engine
