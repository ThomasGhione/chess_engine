#ifndef PIECE_HPP
#define PIECE_HPP

#include "../defines.hpp"
#include "../coords/coords.hpp"
#include <cstdint>

namespace chess {

enum piece_id {
    P_EMPTY = 0,
    P_PAWN = 1,
    P_KNIGHT = 2,
    P_BISHOP = 3,
    P_ROOK = 4,
    P_QUEEN = 5,
    P_KING = 6
};

class Piece {
    
public:
    Piece();
    Piece(Coords c, piece_id i); // for EMPTY squares
    Piece(Coords c, piece_id i, bool color);

    Coords coords;  
    piece_id id;
    bool isWhite;

    bool move(const Coords& c);
    bool move(const uint8_t position);

protected:
    bool isSameColor(const Piece &p) const;

    virtual bool updateLegalMoves();
};

}

#endif