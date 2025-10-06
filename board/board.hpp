#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>
#include "piece/piece.hpp"

/*
    TODO: implement the board
    deve essere di dimensione di una scacchiera (matrice 8x8, array)
    deve "contenere" tutti i pezzi presenti

    board ha un costruttore che parte da un fen
        - variabile costanche che è il fen di partenza                          V
        - puoi partire da ogni posizione in base al fen
    l'inizializzazione del costruttore si deve affidare ad un costruttore
        privato che crea una scacchiera vuota
*/

namespace chess {

class Board {


public:

    Board(std::string fen);

    std::array<Piece, 64> board;
    
private:

    bool isWhiteTurn;
    bool castle[4];
    bool enPassant;
    int halfMoveClock;
    int fullMoveClock;
    const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    

    Board();

    inline uint8_t fromCoordsToPosition(const coords& coords);
    inline coords fromPositionToCoords(const int& position);


};

}

#endif