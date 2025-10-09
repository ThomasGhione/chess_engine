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

/*
    a8->h8, a7->h7, ...
    board[64]
        board[0] => a8
        board[1] => b8
        ...
        board[7] => h8
        board[8] => a7
        board[15] => h7
        board[16] => a6
*/

namespace chess {

class Board {

public:
    std::array<Piece, 64> board;
    
    Board();
    Board(std::string fen);
    
    std::string getCurrentFen();
    
    bool getIsWhiteTurn() const;    
    std::array<bool, 4> getCastling() const;    
    Coords getEnPassant() const;    
    int getHalfMoveClock() const;    
    int getFullMoveClock() const;

    uint8_t fromCoordsToPosition(const Coords& coords) const;
    Coords fromPositionToCoords(const int position) const;

    bool movePiece(const Piece& current, const Coords& target);  

    Board& Board::operator=(const Board& other);
    Piece& operator[](std::size_t index);
    const Piece& operator[](std::size_t index) const;
    
private:
    // fen starts from a8 (top left)
    // (a8->h8, a7->h7, a6->h6, a5->h5, a4->h4, a3->h3, a2->h2, a1->h1)
    // Upper char = white piece
    // Lower char = black piece
    // Numbers = empty squares
    // '/' = new line
    // ' ' = next section
    const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    
    bool isWhiteTurn;
    std::array<bool, 4> castle;
    Coords enPassant;
    int halfMoveClock;
    int fullMoveClock;
    
    void fromFenToBoard(std::string fen);
    std::string fromBoardToFen();
};

}

#endif