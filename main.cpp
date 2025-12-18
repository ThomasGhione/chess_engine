#include "./engine/engine.hpp"
#include "./driver/driver.hpp"

using namespace chess;
using namespace print;
using namespace engine;
using namespace driver;

#include <iostream>
#include <string>

int main() {
    Menu menu = Menu();
    Engine engine = Engine();
    Driver driver = Driver(menu, engine);
    driver.startGame();

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
