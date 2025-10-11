#include "king.hpp"

namespace chess {

King::King(Coords c, piece_id i, bool color)
    : Piece(c, i, color)
{
    legalMoves.reserve(8); // il massimo di mosse legali per un re sono 8
}

void King::getAllLegalMoves(const chessboard& board) {
    int directions[8][2] = { // tutte le direzioni in cui il re può muoversi
        {1, 0}, {0, 1}, {-1, 0}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (auto& dir : directions) {
        
        Coords newPos(coords.file + dir[0], coords.rank + dir[1]);

        if (newPos.file < 8 && newPos.rank < 8 && newPos.file >= 0 && newPos.rank >= 0) { // 0 <= (file,rank) < 8
            const Piece& targetPiece = board[Board::fromCoordsToPosition(newPos)];
            
            if (targetPiece.id == P_EMPTY || !isSameColor(targetPiece)) {
                legalMoves.push_back(newPos);
            }
        }
    }
}

bool King::canMoveTo(const Coords& target) const {
    return std::find(legalMoves.cbegin(), legalMoves.cend(), target) != legalMoves.cend(); 
}

}