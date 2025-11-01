#ifndef KING_HPP
#define KING_HPP

#include <vector>
#include <array>
#include "../coords/coords.hpp"

namespace chess {

class Board; // forward declaration

class King{

public:

    //TODO maybe we need to implement check, checkmate, and stalemate here?
    //TODO implement castling!!

    [[nodiscard]] static std::vector<Coords> getKingMoves(const Board& board, const Coords& from) noexcept;

private:
    static constexpr int directions[8][2] = {
        {1, 0}, {0, 1}, {-1, 0}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
};

}

#endif
