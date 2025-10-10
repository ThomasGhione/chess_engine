#ifndef PRINTS_HPP
#define PRINTS_HPP

#include <string>

#include "../includes.hpp"
#include "../engine/engine.hpp"
#include "../piece/piece.hpp"
#include "../coords/coords.hpp"
#include "../board/board.hpp"

namespace print {

class Prints {
    public:
        Prints();

        static string getPlayer(const bool isWhiteTurn);
        static string getPiece(const chess::Piece& piece);
        static string getPieceOrEmpty(const chess::Board& board, const chess::Coords& coords);
        static void getBoard(const chess::Board& board);
};

}

#endif