#ifndef ROOK_HPP
#define ROOK_HPP

#include "piece.hpp"
#include <algorithm>

namespace chess {

class Rook : public Piece {

public:
    Rook(Coords c, piece_id i, bool color);

    void getAllLegalMoves(const chessboard& board) override;
    bool canMoveTo(const Coords& target) const override;

}; 

}

#endif