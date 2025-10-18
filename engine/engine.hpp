#ifndef ENGINE_HPP
#define ENGINE_HPP

//#include <filesystem>
//#include <fstream>
//#include <unordered_map>
#include <cstdint>
#include <iostream>

// Usata solo per sleep 
// #include <unistd.h>

//#include "../includes.hpp"
#include "../printer/menu.hpp"
#include "../printer/prints.hpp"
#include "../board/board.hpp"
#include "../coords/coords.hpp"

namespace engine {

class Engine final {

//using PieceMovesMap = std::unordered_map<chess::Piece, std::vector<chess::Coords>>;
    
public:
  Engine();


  void startGame();
  void playGameVsHuman();

  //void playGame();
  //void saveGame();
  //bool loadGame();
  //void playGameVsEngine(bool isWhite); // ciclo prinipale
  
  //double eval;

private:
  chess::Board board;

  //bool isPlayerWhite;
  // PieceMovesMap legalMoves;
  //std::unordered_map<chess::Piece, std::vector<chess::Coords>> getLegalMoves();

  void takePlayerTurn();
  bool isMate();
  //void takeEngineTurn(); // ...mossa dopo muove l'engine
  //double evaluate();
  //bool isStalemate();

};
}
#endif // !ENGINE_HPP
