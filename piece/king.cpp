/*
#include "king.hpp"

namespace chess {

// No senso passare ID
//King::King(Coords c, piece_id i, bool color)

// Serve farlo in altro modo
//: Piece(c, i, color)
King::King(Coords c, bool color)
{
  this->coords = c;
  this->isWhite = color;
    legalMoves.reserve(8); // max 8 moves for a king
}

void King::getAllLegalMoves(const std::array<chess::Piece, 64>& board) {
  // Problema di dichiarazioni incrociate

  // Istruzioni messe per evitare waring
  chess::Piece p = board.at(0);
  p.isWhite = true;

    
    legalMoves.clear();

    const Coords start = this->coords;

    for (const auto& dir : directions) {
        Coords newPos(start.file + dir[0], start.rank + dir[1]);
        if (isInBounds(newPos)) {
            const Piece* target = board.at(Board::fromCoordsToPosition(newPos)).get();
            if (!target || !isSameColor(*target)) {
                legalMoves.emplace_back(newPos);
            }
        }
    }
}

bool King::isInCheck(const std::array<chess::Piece, 64>& board) const {
    // Implementation here
  // Istruzioni messe per evitare waring
  chess::Piece p = board.at(0);
  p.isWhite = true;

  return true;
}

bool King::isInCheckmate(const std::array<chess::Piece, 64>& board) const {
    return (isInCheck(board) && legalMoves.empty());
}

}
*/
