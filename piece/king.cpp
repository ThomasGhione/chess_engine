#include "king.hpp"
#include "../board/board.hpp"

namespace chess {

std::vector<Coords> King::getKingMoves(const Board& board, const Coords& from) noexcept {
    std::vector<Coords> legalMoves;
    legalMoves.reserve(8);

    const uint8_t startVal = board.get(from);
    if ((startVal & 0x07) != Board::KING) {
        return legalMoves;
    }

    uint8_t kingColor = board.getColor(from);
    // TODO add check for king's current check status before allowing castling
    if (kingColor == Board::WHITE) {
        if (board.getCastle(0) && !board.getHasMoved(0) && !board.getHasMoved(2)
            && board.getByNoteCoords("f1") == Board::EMPTY
            && board.getByNoteCoords("g1") == Board::EMPTY) {
            // kingside castling logic
            legalMoves.emplace_back(Coords("g1"));
        }
        if (board.getCastle(1) && !board.getHasMoved(0) && !board.getHasMoved(1)
            && board.getByNoteCoords("d1") == Board::EMPTY
            && board.getByNoteCoords("c1") == Board::EMPTY
            && board.getByNoteCoords("b1") == Board::EMPTY) {
            // queenside castling logic
            legalMoves.emplace_back(Coords("c1"));
        }
    }
    else if (kingColor == Board::BLACK) {
        if (board.getCastle(2) && !board.getHasMoved(3) && !board.getHasMoved(5)
            && board.getByNoteCoords("f8") == Board::EMPTY
            && board.getByNoteCoords("g8") == Board::EMPTY) {
            // kingside castling logic
            legalMoves.emplace_back(Coords("g8"));
        }
        if (board.getCastle(3) && !board.getHasMoved(3) && !board.getHasMoved(4)
            && board.getByNoteCoords("d8") == Board::EMPTY
            && board.getByNoteCoords("c8") == Board::EMPTY
            && board.getByNoteCoords("b8") == Board::EMPTY) {
            // queenside castling logic placeholder
            legalMoves.emplace_back(Coords("c8"));
        }
    }

    for (const auto& dir : directions) {
        Coords newPos(static_cast<uint8_t>(from.file + dir[0]), static_cast<uint8_t>(from.rank + dir[1]));
        if (!Coords::isInBounds(newPos)) {
            continue;
        }
        const uint8_t sq = board.get(newPos);
        if (sq == Board::EMPTY || !board.isSameColor(from, newPos)) {
            legalMoves.emplace_back(newPos);
        }
    }

    // TODO: add castling when board state tracks castling rights and check conditions

    return legalMoves;
}

} // namespace chess
