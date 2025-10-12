#include "piece.hpp"

namespace chess {

Piece::Piece()
    : id(P_EMPTY)
{}

Piece::Piece(Coords c, piece_id i)
    : coords(c)
    , id(i)
{}

Piece::Piece(Coords c, piece_id i, bool color)
    : coords(c)
    , id(i)
    , isWhite(color)
{}

bool Piece::isSameColor(const Piece &p) const {
    return isWhite == p.isWhite;
}

bool Piece::canMoveTo(const Coords& target) const {
    return std::find(legalMoves.cbegin(), legalMoves.cend(), target) != legalMoves.cend(); 
}

bool Piece::isInBounds(const Coords& pos) const {
    return pos.file >= 0 && pos.file < 8 && pos.rank >= 0 && pos.rank < 8;
}

}