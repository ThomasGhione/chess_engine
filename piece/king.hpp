#ifndef KING_HPP
#define KING_HPP

#include "piece.hpp"
#include <vector>
#include <algorithm>


namespace chess {

class King : public Piece {

public:
    King(Coords c, piece_id i, bool color);

    void getAllLegalMoves(const chessboard& board) override;
    bool canMoveTo(const Coords& target) const override;

};

}

#endif