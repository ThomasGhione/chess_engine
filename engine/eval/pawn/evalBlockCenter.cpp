#include "../evaluator.hpp"

namespace engine {

namespace {

constexpr uint8_t algebraicSquare(char file, int rank) noexcept {
    return static_cast<uint8_t>((8 - rank) * 8 + (file - 'a'));
}

} // namespace

int32_t Evaluator::evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept {
    static constexpr uint64_t WHITE_D4_PAWN = chess::Board::bitMask(algebraicSquare('d', 4));
    static constexpr uint64_t BLACK_D5_PIECE = chess::Board::bitMask(algebraicSquare('d', 5));
    static constexpr uint64_t WHITE_BLOCKED_KNIGHTS =
        chess::Board::bitMask(algebraicSquare('c', 3)) | chess::Board::bitMask(algebraicSquare('f', 3));
    static constexpr uint64_t WHITE_BLOCKED_BISHOPS =
        chess::Board::bitMask(algebraicSquare('d', 3)) | chess::Board::bitMask(algebraicSquare('e', 3));

    static constexpr uint64_t BLACK_D5_PAWN = chess::Board::bitMask(algebraicSquare('d', 5));
    static constexpr uint64_t WHITE_D4_PIECE = chess::Board::bitMask(algebraicSquare('d', 4));
    static constexpr uint64_t BLACK_BLOCKED_KNIGHTS =
        chess::Board::bitMask(algebraicSquare('c', 6)) | chess::Board::bitMask(algebraicSquare('f', 6));
    static constexpr uint64_t BLACK_BLOCKED_BISHOPS =
        chess::Board::bitMask(algebraicSquare('d', 6)) | chess::Board::bitMask(algebraicSquare('e', 6));

    static constexpr int32_t BLOCKED_CENTER_PENALTY = 15;
    static constexpr int32_t BLOCKED_PIECE_PENALTY = 10;

    int32_t score = 0;

    const bool whiteBlocked = (b.pawns_bb[0] & WHITE_D4_PAWN) && (occ & BLACK_D5_PIECE);
    score -= whiteBlocked * BLOCKED_CENTER_PENALTY;
    score -= whiteBlocked * ((b.knights_bb[0] & WHITE_BLOCKED_KNIGHTS) != 0ULL) * BLOCKED_PIECE_PENALTY;
    score -= whiteBlocked * ((b.bishops_bb[0] & WHITE_BLOCKED_BISHOPS) != 0ULL) * BLOCKED_PIECE_PENALTY;

    const bool blackBlocked = (b.pawns_bb[1] & BLACK_D5_PAWN) && (occ & WHITE_D4_PIECE);
    score += blackBlocked * BLOCKED_CENTER_PENALTY;
    score += blackBlocked * ((b.knights_bb[1] & BLACK_BLOCKED_KNIGHTS) != 0ULL) * BLOCKED_PIECE_PENALTY;
    score += blackBlocked * ((b.bishops_bb[1] & BLACK_BLOCKED_BISHOPS) != 0ULL) * BLOCKED_PIECE_PENALTY;

    return score;
}

} // namespace engine
