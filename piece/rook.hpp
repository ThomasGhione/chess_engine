#ifndef ROOK_HPP
#define ROOK_HPP

#include "piece.hpp"
#include <algorithm>

namespace chess {

class Rook : public Piece {

public:
    bool hasMoved;

    Rook(Coords c, piece_id i, bool color);

    void getAllLegalMoves(const chessboard& board) override final;

private:
    static constexpr int directions[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
}; 

}

#endif