#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>
#include "piece/piece.hpp"

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
