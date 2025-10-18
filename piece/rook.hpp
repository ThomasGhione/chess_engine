#ifndef ROOK_HPP
#define ROOK_HPP

#include <algorithm>
#include <array>
#include "../coords/coords.hpp"

namespace chess {

class Rook{

public:
    Rook(Coords c, piece_id i, bool color);

    void getAllLegalMoves(const std::array<chess::Piece, 64>& board);

private:
    static constexpr int directions[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
}; 

}

#endif
