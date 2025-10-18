#include "driver.hpp"

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"

namespace driver {

    Driver::Driver(print::Menu m, engine::Engine e) 
        : menu(m)
        , engine(e) 
    {}

    void Driver::startGame() {
        while (true) { /*
            u_int8_t choice = menu.mainMenu();

            switch (choice) {
                case 1: {
                    u_int8_t colorChoice = menu.playWithEngineMenu();
                    switch (colorChoice) {
                        case 1:
                            engine.playGameVsEngine(true);
                            break;
                        case 2:
                            engine.playGameVsEngine(false);
                            break;
                        case 3:
                            engine.loadGame();
                            break;
                        case 4:
                            // Back to main menu
                            break;
                        default:
                            std::cout << "Invalid option. Please select a valid option.\n";
                            break;
                    }
                    break;
                }
                case 2:
                    engine.playGameVsHuman();
                    break;
                case 0:
                    std::cout << "\nThank you for playing! Goodbye!\n";
                    exit(EXIT_SUCCESS);
                default:
                    std::cout << "Invalid option. Please select a valid option.\n";
                    break;
            }*/
        } 
    }
}