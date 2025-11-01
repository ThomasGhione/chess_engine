
#ifndef QUEEN_HPP
#define QUEEN_HPP

#include <array>
#include <vector>
#include "../coords/coords.hpp"

namespace chess {

class Board; // forward declaration

class Queen final {

public:
    [[nodiscard]] static std::vector<Coords> getQueenMoves(const Board& board, const Coords& from) noexcept;

private:
    static constexpr int directions[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

};

}

#endif