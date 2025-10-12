#ifndef KING_HPP
#define KING_HPP

#include "piece.hpp"
#include <vector>
#include <algorithm>


namespace chess {

class King : public Piece {

public:
    King(Coords c, piece_id i, bool color);

    void getAllLegalMoves(const chessboard& board) override final;

    bool isInCheck(const chessboard& board) const; // TODO: implementare
    bool isInCheckmate(const chessboard& board) const; // TODO: implementare
private:
    static constexpr int directions[8][2] = { // tutte le direzioni in cui il re può muoversi
        {1, 0}, {0, 1}, {-1, 0}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
};

}

#endif