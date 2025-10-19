#ifndef ROOK_HPP
#define ROOK_HPP

#include <array>
#include <vector>
#include "../board/board.hpp"
#include "../coords/coords.hpp"

namespace chess {

class Rook final {

public:

[[nodiscard]] static std::vector<Coords> getRookMoves(const Board& board, const Coords& from) noexcept {
    
    std::vector<Coords> legalMoves;
    legalMoves.reserve(14);

    const Coords start = from;
    uint8_t startVal = board.get(start);
    
    if (startVal != Board::ROOK)
        return legalMoves; // no piece at source

    // #pragma unroll
    for (const auto& dir : directions) {
        Coords newPos(start.file + dir[0], start.rank + dir[1]);
        while (Coords::isInBounds(newPos)) {
            uint8_t sq = board.get(newPos);
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
    static constexpr int directions[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
};


}

#endif
