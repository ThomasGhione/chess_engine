
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
    // Move generation only (no board side-effects)
    [[nodiscard]] static std::vector<Coords> getPawnMoves(const Board& board, const Coords& from) noexcept;

private:
    static constexpr int directions[4][2] = {
        {1, 0}, {2, 0}, {1, 1}, {1, -1}
    };

};

}

#endif