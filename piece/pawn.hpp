
#ifndef PAWN_HPP
#define PAWN_HPP

#include <iostream>
#include <array>
#include <vector>
#include "../board/board.hpp"
#include "../coords/coords.hpp"

namespace chess {

class Pawn final {

public:


//TODO implement promotion, en passant
[[nodiscard]] static std::vector<Coords> getPawnMoves(Board& board, const Coords& from) noexcept {

    std::vector<Coords> legalMoves;
    legalMoves.reserve(4);

    const Coords start = from;
    Board::piece_id startVal = board.get(start);

    if (startVal != Board::PAWN)
        return legalMoves; // no piece at source

    for (const auto& dir : directions) {
        Coords newPos(start.file + dir[0], start.rank + dir[1]);
        if (Coords::isInBounds(newPos)) {
            Board::piece_id sq = board.get(newPos);
            if (sq == Board::EMPTY || !board.isSameColor(start, newPos)) {
                legalMoves.emplace_back(newPos);
            }
        }

        if (newPos.rank == 7 || newPos.rank == 0) {
            promotePawn(board, newPos, (newPos.rank == 7));
        }
    }

    return legalMoves;
}

private:

    static void promotePawn(Board& board, const Coords& pos, bool isWhite) noexcept {
        std::cout << "Choose a piece to promote your pawn to (q, r, b, n): ";
        char choice;
        std::cin >> choice;

        uint8_t choice = Board::EMPTY;

        while (!choice) {
            switch (std::tolower(choice)) {
                [[likely]] case 'q':
                    choice = Board::QUEEN;
                    break;
                case 'r':
                    choice = Board::ROOK;
                    break;
                case 'b':
                    choice = Board::BISHOP;
                    break;
                case 'n':
                    choice = Board::KNIGHT;
                    break;
                default:
                    std::cout << "Invalid choice for promotion. Please try again\n";
                    std::cin >> choice;
                    break;
            }
        }

        choice |= isWhite ? Board::WHITE : Board::BLACK;

        Board::piece_id newPiece = static_cast<Board::piece_id>(choice); // TODO we need to implement |= operator for piece_id
        
        board.set(pos, newPiece); 
    }


    static constexpr int directions[4][2] = {
        {1, 0}, {2, 0}, {1, 1}, {1, -1}
    };

};

}

#endif