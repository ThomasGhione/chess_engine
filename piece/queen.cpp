#include "queen.hpp"
#include "../board/board.hpp"

namespace chess {

std::vector<Coords> Queen::getQueenMoves(const Board& board, const Coords& from) noexcept {
    std::vector<Coords> legalMoves;
    legalMoves.reserve(27);

    const uint8_t startVal = board.get(from);
    if ((startVal & 0x07) != Board::QUEEN) {
        return legalMoves;
    }

    for (const auto& dir : directions) {
        Coords newPos(static_cast<uint8_t>(from.file + dir[0]), static_cast<uint8_t>(from.rank + dir[1]));
        while (Coords::isInBounds(newPos)) {
            const uint8_t sq = board.get(newPos);
            if (sq != Board::EMPTY) {
                if (!board.isSameColor(from, newPos)) {
                    legalMoves.emplace_back(newPos);
                }
                break;
            }
            legalMoves.emplace_back(newPos);
            const int nf = static_cast<int>(newPos.file) + dir[0];
            const int nr = static_cast<int>(newPos.rank) + dir[1];
            newPos = Coords(static_cast<uint8_t>(nf), static_cast<uint8_t>(nr));
        }
    }

    return legalMoves;
}

} // namespace chess
