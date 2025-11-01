
#ifndef PAWN_HPP
#define PAWN_HPP

#include <iostream>
#include <array>
#include <vector>
#include "../coords/coords.hpp"

namespace chess {

class Board; // forward declaration

class Pawn final {

public:
    //TODO implement promotion, en passant
    [[nodiscard]] static std::vector<Coords> getPawnMoves(Board& board, const Coords& from) noexcept;

private:
    static void promotePawn(Board& board, const Coords& pos, bool isWhite) noexcept;

    static constexpr int directions[4][2] = {
        {1, 0}, {2, 0}, {1, 1}, {1, -1}
    };

};

}

#endif