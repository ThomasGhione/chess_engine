#include "./engine/engine.hpp"
#include "./driver/driver.hpp"

using namespace chess;
using namespace print;
using namespace engine;
using namespace driver;

#include <iostream>
#include <string>

// ./chess -> Goes to main menu
// ./chess bvb -> Bot vs Bot
// ./chess pvp -> Player vs Player
// ./chess pvb w -> Player vs Bot (player is white)
// ./chess pvb b -> Player vs Bot (player is black)
// ./chess bvs w -> Bot vs Stockfish (bot is white)
// ./chess bvs b -> Bot vs Stockfish (bot is black)
// ./chess 11 -> Option 1 (player vs bot) - Option 1 (white)
// ./chess 12 -> Option 1 (player vs bot) - Option 2 (black)


int main(int argc, char *argv[]) {
    Menu menu = Menu();
    Engine engine = Engine();
    
    Driver driver = Driver(menu, engine);
    driver.startGame(argc, argv);
    return 0;
}
