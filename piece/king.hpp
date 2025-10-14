#ifndef KING_HPP
#define KING_HPP

#include "piece.hpp"
#include <vector>
#include <algorithm>
#include <array>


namespace chess {

class King : public Piece {

public:
    //King(Coords c, piece_id i, bool color);
    King(Coords c, bool color);

    void getAllLegalMoves(const std::array<chess::Piece, 64>& board);

    bool isInCheck(const std::array<chess::Piece, 64>& board) const; // TODO: implement
    bool isInCheckmate(const std::array<chess::Piece, 64>& board) const; // TODO: implement
private:
    static constexpr int directions[8][2] = { // All directions the king can move
        {1, 0}, {0, 1}, {-1, 0}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
};

}

#endif
