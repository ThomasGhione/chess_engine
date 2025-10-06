#ifndef PRINTS_HPP
#define PRINTS_HPP

#include <string>

#include "includes.hpp"
#include "chessengine.hpp"
#include "piece.hpp"
#include "coords.hpp"
#include "board.hpp"

namespace print {

class Prints {
    public:
        Prints();

        static string getPlayer(const bool isWhiteTurn);
        static string getPiece(const chess::Piece& piece);
        static string getPieceOrEmpty(const chess::Board& board, const chess::coords& coords);
        static void getBoard(chess::gameStatus &gs);

};

}

#endif