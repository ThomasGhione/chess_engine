#include "knight.hpp"

namespace chess {

// Se stiamo parlando di un cavallo, id è costante, perché dovrebbe essere un parametro?
// Knight::Knight(Coords c, piece_id i, bool color)

Knight::Knight(Coords c, bool color)
    : Piece(c, P_KNIGHT, color)
{}

void Knight::getAllLegalMoves(const std::array<chess::Piece, 64>& board) {
  // Anche qui abbiamo un problema di dichiarazioni incrociate
  
  // Istruzioni messe per evitare waring
  chess::Piece p = board.at(0);
  p.isWhite = true;

  /*
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
    */
}

}
