#ifndef ENGINE_HPP
#define ENGINE_HPP

//#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>

// Usata solo per sleep 
// #include <unistd.h>

#include "../printer/menu.hpp"
#include "../printer/prints.hpp"
#include "../board/board.hpp"
#include "../coords/coords.hpp"

namespace engine {

class Engine final {

public:
    Engine();

    chess::Board board;
    bool isPlayerWhite;

    static int64_t globalEval;
    uint64_t depth;

    int64_t eval;

    void playGameVsHuman();
    void playGameVsEngine(bool isWhite); // ciclo prinipale
  

    void search(uint64_t depth);
    int64_t evaluate(); 


private:

    void takePlayerTurn();
    bool isMate();
    int64_t getMaterialDelta(chess::Board b);

    //void takeEngineTurn(); // ...mossa dopo muove l'engine

};

} 

#endif
