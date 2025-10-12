#include "rook.hpp"

namespace chess {

Rook::Rook(Coords c, piece_id i, bool color)
    : Piece(c, i, color)
{
    legalMoves.reserve(14);
}

void Rook::getAllLegalMoves(const chessboard& board) {

    // generate the logic of the rook:
    legalMoves.clear();
    static const int directions[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} }; // right, left, down, up

    for (const auto& dir : directions) {
        int x = coords.x + dir[0];
        int y = coords.y + dir[1];
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            const Piece* target = board.getPieceAt({x, y});
            if (target == nullptr) {
                legalMoves.push_back({x, y});
            } else {
                if (target->getColor() != this->getColor()) {
                    legalMoves.push_back({x, y});
                }
                break;
            }
            x += dir[0];
            y += dir[1];
        }
    }

}

bool Rook::canMoveTo(const Coords& target) const {
    return std::find(legalMoves.cbegin(), legalMoves.cend(), target) != legalMoves.cend(); 
}

}