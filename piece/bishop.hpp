#ifndef BISHOP_HPP
#define BISHOP_HPP

#include <array>
#include <vector>
#include "../coords/coords.hpp"

namespace chess {

class Board; // forward declaration

class Bishop final {

public:
    [[nodiscard]] static std::vector<Coords> getBishopMoves(const Board& board, const Coords& from) noexcept;

private:
    static constexpr int directions[4][2] = { {1,1}, {-1,-1}, {1,-1}, {-1,1} };
};


}

#endif
