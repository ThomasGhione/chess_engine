#ifndef PRINTS_HPP
#define PRINTS_HPP

#include "../includes.hpp"

namespace print {

class Prints {
    public:
        Prints();

        static string getPlayer(const player &player) noexcept;
        static string getPiece(piece_id piece) noexcept;
        static string getPieceOrEmpty(const board debugboard[ML][ML], const int rank, const int file) noexcept;
        static void getBoard(gameStatus &gs) noexcept;

};

}

#endif