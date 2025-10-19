#ifndef KING_HPP
#define KING_HPP

#include <vector>
#include <array>
#include "../coords/coords.hpp"
#include "../board/board.hpp"

namespace chess {

class King{

public:

//TODO maybe we need to implement check, checkmate, and stalemate here?

//TODO implement castling!!

[[nodiscard]] static std::vector<Coords> getKingMoves(const Board& board, const Coords& from) noexcept {

    std::vector<Coords> legalMoves;
    legalMoves.reserve(27);

    const Coords start = from;
    uint8_t startVal = board.get(start);

    if (startVal != Board::KING)
        return legalMoves; // no piece at source

    //TODO S maybe there's a way not to loop?
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
    static constexpr int directions[8][2] = {
        {1, 0}, {0, 1}, {-1, 0}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
};

}

#endif
