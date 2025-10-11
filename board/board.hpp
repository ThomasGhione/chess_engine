#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>
#include "../piece/piece.hpp"

namespace chess {

using chessboard = std::array<Piece, 64>;

class Board {

public:
    chessboard board;
    
    // Default constructor => starting position
    Board();
    Board(std::string fen);
    
    std::string getCurrentFen();
    
    bool getIsWhiteTurn() const;
    std::array<bool, 4> getCastling() const;
    Coords getEnPassant() const;
    int getHalfMoveClock() const;
    int getFullMoveClock() const;

    static uint8_t fromCoordsToPosition(const Coords& coords);
    static Coords fromPositionToCoords(const int position);

    bool movePiece(const Piece& current, const Coords& target);

    Board& Board::operator=(const Board& other);
    Piece& operator[](std::size_t index);
    const Piece& operator[](std::size_t index) const;

    Piece& at(const uint8_t position);
    const Piece& at(const uint8_t position) const;
    
private:
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
