#include "king.hpp"
#include "../board/board.hpp"

namespace chess {

std::vector<Coords> King::getKingMoves(const Board& board, const Coords& from) noexcept {
    std::vector<Coords> legalMoves;
    legalMoves.reserve(8);

    const uint8_t startVal = board.get(from);
    if ((startVal & 0x07) != Board::KING) {
        return legalMoves;
    }

    for (const auto& dir : directions) {
        Coords newPos(static_cast<uint8_t>(from.file + dir[0]), static_cast<uint8_t>(from.rank + dir[1]));
        if (!Coords::isInBounds(newPos)) {
            continue;
        }
        const uint8_t sq = board.get(newPos);
        if (sq == Board::EMPTY || !board.isSameColor(from, newPos)) {
            legalMoves.emplace_back(newPos);
        }
    }

    // TODO: add castling when board state tracks castling rights and check conditions

    return legalMoves;
}

} // namespace chess
