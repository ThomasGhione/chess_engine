#ifndef KNIGHT_HPP
#define KNIGHT_HPP

#include "piece.hpp"

namespace chess {
    
class Knight : public Piece {
    public:
        Knight(Coords c, piece_id i, bool color);

        void getAllLegalMoves(const chessboard& board) override final;
    
    private:
        static constexpr int directions[8][2] = {
            {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
            {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
        };
};

}

#endif