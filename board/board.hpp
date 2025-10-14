#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
//#include <array>
//#include <cstdint>
//#include <sstream>
//#include <cctype>
//#include <algorithm>
//#include "../piece/piece.hpp"

namespace chess {

class Board {

public:
  // Default constructor => starting position
  Board();
    
  bool isCurrentPositionMate();
  std::string getCurrentPositionFen();

    //std::array<chess::Piece, 64> board;
    //// Board(int empty = EMPTY); Note: Non possiamo avere il costruttore fatto in questo modo, chiedi a Daniele.
    ////Board(int empty);
    //Board(std::string fen);
    //
    //std::string getCurrentFen();
    //
    //bool getIsWhiteTurn() const {return this->isWhiteTurn;}
    //std::array<bool, 4> getCastling() const {return this->castle;}
    //chess::Coords getEnPassant() const {return this->enPassant;}
    //int getHalfMoveClock() const {return this->halfMoveClock;}
    //int getFullMoveClock() const {return this->fullMoveClock;}

    //static uint8_t fromCoordsToPosition(const Coords& coords);
    //static Coords fromPositionToCoords(const int position);

    //bool movePiece(const Piece& current, const Coords& target);

    //Board& operator=(const chess::Board& other);
    //Piece& operator[](std::size_t index);
    //const Piece& operator[](std::size_t index) const;

    //Piece& at(const uint8_t position);
    //const Piece& at(const uint8_t position) const;
    
private:
    //static const std::string STARTING_FEN;
    //
    //bool isWhiteTurn;
    //std::array<bool, 4> castle;
    //Coords enPassant;
    //int halfMoveClock;
    //int fullMoveClock;
    //
    //void fromFenToBoard(std::string fen);
};

}

#endif
