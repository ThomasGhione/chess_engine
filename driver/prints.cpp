#include "driver.hpp"

namespace driver {

using namespace chess;

static inline char pieceToSymbol(uint8_t piece) noexcept {
    if (piece == Board::EMPTY) return '.';

    const bool isBlack = (piece & Board::BLACK) != 0;
    switch (piece & Board::MASK_PIECE_TYPE) {
        case Board::PAWN:   return isBlack ? 'p' : 'P';
        case Board::KNIGHT: return isBlack ? 'n' : 'N';
        case Board::BISHOP: return isBlack ? 'b' : 'B';
        case Board::ROOK:   return isBlack ? 'r' : 'R';
        case Board::QUEEN:  return isBlack ? 'q' : 'Q';
        case Board::KING:   return isBlack ? 'k' : 'K';
        default:            return '?';
    }
}

std::string Driver::getBasicBoard(const Board& board) {
    std::string result = "  a b c d e f g h\n";
    for (int row = 7; row >= 0; --row) {
        result += std::to_string(row + 1) + " ";
        for (int col = 0; col < 8; ++col) {
            result += pieceToSymbol(board.get(row, col));
            result += ' ';
        }
        result += " " + std::to_string(row + 1) + "\n";
    }
    result += "  a b c d e f g h\n";
    return result;
}


} // namespace driver
