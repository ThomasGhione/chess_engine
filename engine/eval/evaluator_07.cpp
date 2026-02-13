#include "evaluator.hpp"

namespace engine {

int64_t Evaluator::evaluateCheckmate(const chess::Board& board) noexcept {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

int64_t Evaluator::evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    // Central squares: d4, e4, d5, e5 (indices 27, 28, 35, 36)
    static constexpr uint64_t CENTER_MASK = 
        chess::Board::bitMask(27) | chess::Board::bitMask(28) |
        chess::Board::bitMask(35) | chess::Board::bitMask(36);
    
    const int whiteControl = __builtin_popcountll(whitePawns & CENTER_MASK);
    const int blackControl = __builtin_popcountll(blackPawns & CENTER_MASK);
    
    return (whiteControl - blackControl) * CENTER_CONTROL_BONUS;
}

int64_t Evaluator::evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept {
    static constexpr uint64_t WHITE_D4_PAWN = chess::Board::bitMask(27);
    static constexpr uint64_t BLACK_D5_PIECE = chess::Board::bitMask(35);
    static constexpr uint64_t WHITE_BLOCKED_KNIGHTS = chess::Board::bitMask(18) | chess::Board::bitMask(21);
    static constexpr uint64_t WHITE_BLOCKED_BISHOPS = chess::Board::bitMask(19) | chess::Board::bitMask(20);
    
    static constexpr uint64_t BLACK_D5_PAWN = chess::Board::bitMask(35);
    static constexpr uint64_t WHITE_D4_PIECE = chess::Board::bitMask(27);
    static constexpr uint64_t BLACK_BLOCKED_KNIGHTS = chess::Board::bitMask(42) | chess::Board::bitMask(45);
    static constexpr uint64_t BLACK_BLOCKED_BISHOPS = chess::Board::bitMask(43) | chess::Board::bitMask(44);
    
    static constexpr int64_t BLOCKED_CENTER_PENALTY = 15;
    static constexpr int64_t BLOCKED_PIECE_PENALTY = 10;
    
    int64_t score = 0;
    
    // WHITE: d4 pawn blocked by piece on d5
    const bool whiteBlocked = (b.pawns_bb[0] & WHITE_D4_PAWN) && (occ & BLACK_D5_PIECE);
    score -= whiteBlocked * BLOCKED_CENTER_PENALTY;
    score -= whiteBlocked * static_cast<bool>(b.knights_bb[0] & WHITE_BLOCKED_KNIGHTS) * BLOCKED_PIECE_PENALTY;
    score -= whiteBlocked * static_cast<bool>(b.bishops_bb[0] & WHITE_BLOCKED_BISHOPS) * BLOCKED_PIECE_PENALTY;

    // BLACK: d5 pawn blocked by piece on d4
    const bool blackBlocked = (b.pawns_bb[1] & BLACK_D5_PAWN) && (occ & WHITE_D4_PIECE);
    score += blackBlocked * BLOCKED_CENTER_PENALTY;
    score += blackBlocked * static_cast<bool>(b.knights_bb[1] & BLACK_BLOCKED_KNIGHTS) * BLOCKED_PIECE_PENALTY;
    score += blackBlocked * static_cast<bool>(b.bishops_bb[1] & BLACK_BLOCKED_BISHOPS) * BLOCKED_PIECE_PENALTY;

    return score;
}

} // namespace engine
