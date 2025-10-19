#include "driver.hpp"

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"

namespace driver {

    Driver::Driver(print::Menu m, engine::Engine e) 
        : menu(m)
        , engine(e) 
    {}

    void Driver::startGame() {
        while (true) {
            uint8_t mainMenuChoice = menu.mainMenu();

            switch (mainMenuChoice) {
                
                case '1': {
                    uint8_t colorChoice = menu.playWithEngineMenu();
                    switch (colorChoice) {
                        case '1':
                        engine.playGameVsEngine(true);
                        break;
                        case '2':
                        engine.playGameVsEngine(false);
                        break;
                        case '3':
                        engine.loadGame(false);
                        break;
                        case '4':
                        // Back to main menu
                        break;
                        default:
                        std::cout << "Invalid option. Please select a valid option.\n";
                        break;
                    }
                    break;
                }

                case '2': {
                    uint8_t twoPlayerChoice = menu.playWithPlayerMenu();
                    switch (twoPlayerChoice) {
                        case '1':
                        engine.playGameVsHuman();
                        break;
                        case '2':
                        engine.loadGame(true);
                        break;
                        case '3':
                        // Back to main menu
                        break;
                        default:
                        std::cout << "Invalid option. Please select a valid option.\n";
                        break;
                    }
                    break;
                }
                
                case '3':
                    std::cout << "\nThank you for playing! Goodbye!\n";
                    exit(EXIT_SUCCESS);
                    break;

                default:
                std::cout << "Invalid option. Please select a valid option.\n";
                break;
            }
        } 
    }
}