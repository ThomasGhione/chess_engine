#ifndef ROOK_HPP
#define ROOK_HPP

#include <array>
#include <vector>
#include "../coords/coords.hpp"

namespace chess {

class Board; // forward declaration

class Rook final {

public:
    [[nodiscard]] static std::vector<Coords> getRookMoves(const Board& board, const Coords& from) noexcept;

private:
    static constexpr int directions[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
};


}

#endif
