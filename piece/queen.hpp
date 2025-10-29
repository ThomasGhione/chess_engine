
#ifndef QUEEN_HPP
#define QUEEN_HPP

#include <array>
#include <vector>
#include "../board/board.hpp"
#include "../coords/coords.hpp"

namespace chess {

class Queen final {

public:

[[nodiscard]] static std::vector<Coords> getQueenMoves(const Board& board, const Coords& from) noexcept {

    std::vector<Coords> legalMoves;
    legalMoves.reserve(27);

    const Coords start = from;
    Board::piece_id startVal = board.get(start);

    if (startVal != Board::QUEEN)
        return legalMoves; // no piece at source

    for (const auto& dir : directions) {
        Coords newPos(start.file + dir[0], start.rank + dir[1]);
        while (Coords::isInBounds(newPos)) {
            Board::piece_id sq = board.get(newPos);
            if (sq != Board::EMPTY) {
                if (!board.isSameColor(start, newPos)) {
                    legalMoves.emplace_back(newPos);
                }
                break;
            }
            legalMoves.emplace_back(newPos);
            newPos.update(newPos.file + dir[0], newPos.rank + dir[1]);
        }
    }

    return legalMoves;
}

private:
    static constexpr int directions[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

};

}

#endif