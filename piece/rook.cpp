#include "rook.hpp"

namespace chess {

Rook::Rook(Coords c, piece_id i, bool color)
    : Piece(c, i, color)
{
    legalMoves.reserve(14);
}

void Rook::getAllLegalMoves(const std::array<chess::Piece, 64>& board) {
  // Abbiamo un problema di inclusione incrociata!
  
  // Istruzioni messe per evitare waring
  chess::Piece p = board.at(0);
  p.isWhite = true;
  /*  
  legalMoves.clear();
    
    //const bool myColor = this->isWhite;
    const Coords start = this->coords;

    for (const auto& dir : directions) {
        Coords newPos(start.file + dir[0], start.rank + dir[1]);
        while (isInBounds(newPos)) {
            auto& square = board.at(Board::fromCoordsToPosition(newPos));
            const Piece* target = square.get();
            if (!target) {
                legalMoves.emplace_back(newPos);
            } else {
                if (!isSameColor(*target)) {
                    legalMoves.emplace_back(newPos);
                }
                break;
            }
            newPos.update(newPos.file + dir[0], newPos.rank + dir[1]);
        }
    }
    */
}

}
