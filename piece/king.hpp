#ifndef KING_HPP
#define KING_HPP

#include "piece.hpp"
#include <vector>
#include <algorithm>


namespace chess {

class King : public Piece {

public:
    King(Coords c, piece_id i, bool color);

    void getAllLegalMoves(const chessboard& board) override;
    bool canMoveTo(const Coords& target) const override;

    bool isInCheck(const chessboard& board) const; // TODO: implementare
    bool isInCheckmate(const chessboard& board) const; // TODO: implementare
private:
    static const int directions[8][2]; // tutte le direzioni in cui il re può muoversi
};

}

#endif