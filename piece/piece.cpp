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

bool Piece::move(const Coords& c) {
    return true;
}
bool Piece::move(const uint8_t position) {
    return true;
}

}