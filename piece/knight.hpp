
#ifndef KNIGHT_HPP
#define KNIGHT_HPP

#include <array>
#include <vector>
#include "../coords/coords.hpp"

namespace chess {

class Board; // forward declaration to avoid circular include

class Knight final {

public:
    [[nodiscard]] static std::vector<Coords> getKnightMoves(const Board& board, const Coords& from) noexcept;

private:
    static constexpr int directions[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
};

}

#endif