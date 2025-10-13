#ifndef PIECE_HPP
#define PIECE_HPP

#include "../defines.hpp"
#include "../coords/coords.hpp"
#include "../board/board.hpp"
#include <cstdint>
#include <vector>
#include <set>
#include <algorithm>

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

using chessboard = std::array<chess::Piece, 64>;

public:
    Piece();
    Piece(Coords c, piece_id i); // for EMPTY squares
    Piece(Coords c, piece_id i, bool color);
    Piece(const chess::Piece& p);

    Coords coords;  
    piece_id id;
    bool isWhite;
    bool hasMoved = false; // Tracks if the piece has moved
    std::vector<Coords> legalMoves; // Legal moves for the engine

    //virtual void getAllLegalMoves(const chessboard& board) = 0; // Pure virtual for derived classes

    bool canMoveTo(const Coords& target) const; // Check if target is in legalMoves

    Piece& operator=(const chess::Piece& p);
protected:
    bool isSameColor(const Piece &p) const; // Check if two pieces have the same color

    // Utility methods for derived classes
    bool isInBounds(const Coords& pos) const; // Check if position is within board bounds
};

}

#endif
