#ifndef PRINTS_HPP
#define PRINTS_HPP

#include <string>
#include <iostream>
#include "../board/board.hpp"

//#include "../includes.hpp"
//#include "../engine/engine.hpp"
//#include "../piece/piece.hpp"
//#include "../coords/coords.hpp"
//#include "../board/board.hpp"

namespace print {

class Prints {
public:
  //Prints();

  //static std::string getPlayer(const bool isWhiteTurn);
  //static std::string getPiece(const chess::Piece& piece);
  //static std::string getPieceOrEmpty(const chess::Board& board, const chess::Coords& coords);
  //static void getBoard(const chess::Board& board);
  static std::string getPrintableBoard(const std::string& FEN);
  static std::string getBasicBoard(const chess::Board& board);
};

}

#endif