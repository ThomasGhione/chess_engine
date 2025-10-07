#ifndef KNIGHT_HPP
#define KNIGHT_HPP

#include "piece.hpp"

namespace chess {
    
class Knight : public Piece {
    public:
        Knight(Coords c, piece_id i, bool color) : Piece(c, P_KNIGHT, color) {}

        bool updateLegalMoves() override;
};

}

#endif