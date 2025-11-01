#include "pawn.hpp"
#include "../board/board.hpp"
#include <cctype>

namespace chess {

std::vector<Coords> Pawn::getPawnMoves(Board& board, const Coords& from) noexcept {
    std::vector<Coords> legalMoves;
    legalMoves.reserve(4);

    const uint8_t startVal = board.get(from);
    if ((startVal & 0x07) != Board::PAWN) {
        return legalMoves;
    }

    const bool isWhite = (board.getColor(from) == Board::WHITE);
    const int dir = isWhite ? 1 : -1;
    const int startRank = isWhite ? 1 : 6;

    // one step forward
    Coords oneStep(static_cast<uint8_t>(from.file), static_cast<uint8_t>(from.rank + dir));
    if (Coords::isInBounds(oneStep) && board.get(oneStep) == Board::EMPTY) {
        legalMoves.emplace_back(oneStep);

        // two steps from starting rank if path clear
        if (from.rank == startRank) {
            Coords twoStep(static_cast<uint8_t>(from.file), static_cast<uint8_t>(from.rank + 2 * dir));
            if (Coords::isInBounds(twoStep) && board.get(twoStep) == Board::EMPTY) {
                legalMoves.emplace_back(twoStep);
            }
        }
    }

    // captures
    for (int dx : { -1, 1 }) {
        const int nf = static_cast<int>(from.file) + dx;
        const int nr = static_cast<int>(from.rank) + dir;
        Coords cap(static_cast<uint8_t>(nf), static_cast<uint8_t>(nr));
        if (!Coords::isInBounds(cap)) continue;
        const uint8_t sq = board.get(cap);
        if (sq != Board::EMPTY && !board.isSameColor(from, cap)) {
            legalMoves.emplace_back(cap);
        }
    }

    // TODO: en passant and promotion handling (non-interactive)
    return legalMoves;
}

void Pawn::promotePawn(Board& board, const Coords& pos, bool isWhite) noexcept {
    // Non-interactive default: promote to Queen
    uint8_t piece = Board::QUEEN | (isWhite ? Board::WHITE : Board::BLACK);
    board.set(pos, static_cast<Board::piece_id>(piece));
}

} // namespace chess
