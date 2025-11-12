#ifndef ENGINE_HPP
#define ENGINE_HPP

//#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>

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

    chess::Board board;
    bool isPlayerWhite;

    int64_t globalEval;
    uint64_t depth;

    void playGameVsHuman();
    void playGameVsEngine(bool isWhite); // ciclo prinipale
  

    void search();
    int64_t evaluate(); 


private:

    void takePlayerTurn();
    bool isMate();
    int64_t getMaterialDelta(chess::Board b);

    //void takeEngineTurn(); // ...mossa dopo muove l'engine
    //bool isStalemate();

};
}

#endif
