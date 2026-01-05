#include "./engine/engine.hpp"
#include "./driver/driver.hpp"

using namespace chess;
using namespace print;
using namespace engine;
using namespace driver;

#include <iostream>
#include <string>

// ./chess bvb
// ./chess pvp
// ./chess pvb w
// ./chess pvb b 
// ./chess 11 -> Option 1 (player vs bot) - Option 1 (white)
// ./chess 12 -> Option 1 (player vs bot) - Option 2 (black)


int main(int argc, char *argv[]) {
    Menu menu = Menu();
    Engine engine = Engine();
    Driver driver = Driver(menu, engine);
    
	driver.startGame(argc, argv);

    /*
    // Bot Vs Bot
    std::string currentBoard;
    for(int i = 0; i < 50; i++) {
        driver.engineTurn();
        if (driver.engine.isCheckMate) { return 1;}

        currentBoard = print::Prints::getBasicBoard(driver.engine.board);
        std::cout << currentBoard << "\n";

        driver.engineTurn();
        if (driver.engine.isCheckMate) { return 2; }

        currentBoard = print::Prints::getBasicBoard(driver.engine.board);
        std::cout << currentBoard << "\n";
    }
    */

    return 0;
}
