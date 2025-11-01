#include "knight.hpp"
#include "../board/board.hpp"

namespace chess {

std::vector<Coords> Knight::getKnightMoves(const Board& board, const Coords& from) noexcept {
    std::vector<Coords> legalMoves;
    legalMoves.reserve(8);

    const Coords start = from;
    const uint8_t startVal = board.get(start);

    // piece type mask 0x07; knight id 0x02
    if ((startVal & 0x07) != Board::KNIGHT) {
        return legalMoves; // not a knight at source
    }

    for (const auto& dir : directions) {
        Coords newPos(static_cast<uint8_t>(start.file + dir[0]), static_cast<uint8_t>(start.rank + dir[1]));
        if (!Coords::isInBounds(newPos)) {
            continue;
        }
        const uint8_t sq = board.get(newPos);
        if (sq == Board::EMPTY || !board.isSameColor(start, newPos)) {
            legalMoves.emplace_back(newPos);
        }
    }

    return legalMoves;
}

} // namespace chess
