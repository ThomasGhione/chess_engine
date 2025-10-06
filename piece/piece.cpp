#include "piece.hpp"
#include "coords.hpp"

namespace chess {

Piece::Piece()
    : id(P_EMPTY)
{}
Piece::Piece(struct coords c, piece_id i, bool color)
    : coords(c)
    , id(i)
    , isWhite(color)
{}

void Piece::move(struct coords c) {
    auto valid_coords = [](uint8_t x) -> bool { return x >= 0 && x < 64; };

    if (valid_coords(c.file)) {
        coords.file = c.file;
    }
    if (valid_coords(c.rank)) {
        coords.rank = c.rank;
    }
}

bool Piece::isSameColor(const Piece &p) const {
    return isWhite == p.isWhite;
}

}