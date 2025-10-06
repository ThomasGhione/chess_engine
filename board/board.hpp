#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>

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

class Board {


public:
    Board(std::string fen);

private:
    const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    
    std::array<uint8_t, 64> board; // TODO: cambiare a <piece , 64>
    struct Coords {
        uint8_t row;
        uint8_t col;
    };

    Board();

    inline uint8_t fromCoordsToPosition(const Coords& coords);
    inline Coords fromPositionToCoords(const int& position);


};

#endif