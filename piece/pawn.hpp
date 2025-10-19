
#ifndef PAWN_HPP
#define PAWN_HPP

#include <array>
#include <vector>
#include "../board/board.hpp"
#include "../coords/coords.hpp"

namespace chess {

class Pawn final {

public:


//TODO implement promotion, en passant
[[nodiscard]] static std::vector<Coords> getPawnMoves(const Board& board, const Coords& from) noexcept {

    std::vector<Coords> legalMoves;
    legalMoves.reserve(4);

    const Coords start = from;
    uint8_t startVal = board.get(start);

    if (startVal != Board::PAWN)
        return legalMoves; // no piece at source

    for (const auto& dir : directions) {
        Coords newPos(start.file + dir[0], start.rank + dir[1]);
        if (Coords::isInBounds(newPos)) {
            uint8_t sq = board.get(newPos);
            if (sq == Board::EMPTY || !board.isSameColor(start, newPos)) {
                legalMoves.emplace_back(newPos);
            }
        }
    }

    return legalMoves;
}

private:
    static constexpr int directions[4][2] = {
        {1, 0}, {2, 0}, {1, 1}, {1, -1}
    };

};

}

#endif