#include "knight.hpp"

namespace chess {

Knight::Knight(Coords c, piece_id i, bool color)
    : Piece(c, P_KNIGHT, color)
{}

void Knight::getAllLegalMoves(const chessboard& board) {
    legalMoves.clear();

    for (const auto& dir : directions) {
        Coords newPos(coords.file + dir[0], coords.rank + dir[1]);
        if (isInBounds(newPos)) {
            const Piece* target = board.at(Board::fromCoordsToPosition(newPos)).get();
            if (!target || !isSameColor(*target)) {
                legalMoves.emplace_back(newPos);
            }
        }
    }
}

}