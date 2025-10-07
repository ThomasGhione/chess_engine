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

    std::array<Piece, 64> board;
    bool isWhiteTurn;
    std::array<bool, 4> castle;
    Coords enPassant;
    int halfMoveClock;
    int fullMoveClock;
    
    Board(std::string fen);

    std::string getCurrentFen();

    Board& Board::operator=(const Board& other);

private:

    const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";


    Board();
    
    void fromFenToBoard(std::string fen);
    std::string fromBoardToFen();

    inline uint8_t fromCoordsToPosition(const Coords& coords);
    inline Coords fromPositionToCoords(const int& position);

    void updatePiecePosition(const Piece& current, const Coords& target);  
};

}

#endif