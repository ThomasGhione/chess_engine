#include "piece.hpp"

namespace chess {

Piece::Piece()
    : id(P_EMPTY)
{}

// Per poter fare:     : coords(c)
// Serve anche il costruttore!

Piece::Piece(Coords c, piece_id i): id(i){
  this->coords = c;
}

Piece::Piece(Coords c, piece_id i, bool color) : id(i)
  , isWhite(color){
  this->coords = c;
}

bool Piece::isSameColor(const Piece &p) const {
    return isWhite == p.isWhite;
}

bool Piece::canMoveTo(const Coords& target) const {
    return std::find(legalMoves.cbegin(), legalMoves.cend(), target) != legalMoves.cend(); 
}

bool Piece::isInBounds(const Coords& pos) const {
    return pos.file < 8 && pos.rank < 8;
}

}
