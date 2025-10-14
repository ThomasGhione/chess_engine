#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <cstdint>
#include <iostream>


//#include "../board/board.hpp"
//#include "../includes.hpp"
#include "../printer/menu.hpp"

namespace engine {
class Engine {

//using PieceMovesMap = std::unordered_map<chess::Piece, std::vector<chess::Coords>>;
    
public:
  Engine();

  //double eval;

  //void playGameVsEngine(bool isWhite); // ciclo prinipale
  void playGameVsHuman(); // same as above :)

  //void playGame();
  //void saveGame();
  //bool loadGame();

  void startGame();

private:
  //chess::Board board;
  //bool isPlayerWhite;
  
  // PieceMovesMap legalMoves;

  //std::unordered_map<chess::Piece, std::vector<chess::Coords>> getLegalMoves();

  void takePlayerTurn(); // muove il giocatore...
  //void takeEngineTurn(); // ...mossa dopo muove l'engine

  //double evaluate();

  bool isMate();
  //bool isStalemate();
};
}
#endif // !ENGINE_HPP
