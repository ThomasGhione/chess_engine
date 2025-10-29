
#ifndef KNIGHT_HPP
#define KNIGHT_HPP

#include <array>
#include <vector>
#include "../board/board.hpp"
#include "../coords/coords.hpp"

namespace chess {

class Knight final {

public:

[[nodiscard]] static std::vector<Coords> getKnightMoves(const Board& board, const Coords& from) noexcept {

    std::vector<Coords> legalMoves;
    legalMoves.reserve(8);

    const Coords start = from;
    Board::piece_id startVal = board.get(start);

    if (startVal != Board::KNIGHT)
        return legalMoves; // no piece at source

    for (const auto& dir : directions) {
        Coords newPos(start.file + dir[0], start.rank + dir[1]);
        if (Coords::isInBounds(newPos)) {
            Board::piece_id sq = board.get(newPos);
            if (sq == Board::EMPTY || !board.isSameColor(start, newPos)) {
                legalMoves.emplace_back(newPos);
            }
        }
    }

    return legalMoves;
}

private:
    static constexpr int directions[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };

};

}

#endif