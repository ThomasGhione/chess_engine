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
                            loadGame(false);
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
                            loadGame(true);
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

    bool Driver::loadGame(bool isWithPlayer) {
        std::ifstream SaveFile("saves/save.txt");
        if (!SaveFile.is_open()) {
            std::cerr << "Error: Unable to open save file.\n";
            return false;
        }

        std::string line;
    
        if (std::getline(SaveFile, line)) {
            this->engine.board = chess::Board(line);
        } 

	    // TODO aggiungere controlli/eccezioni per il fen
	
	    SaveFile.close();

        if (isWithPlayer) {
            this->engine.playGameVsHuman();
        } else {
            std::cout << "Select your color:\n1. White\n2. Black\n";
            int choice;
            std::cin >> choice;
            if (choice == 1) {
                this->engine.isPlayerWhite = true;
            } else {
                this->engine.isPlayerWhite = false;
            }

            this->engine.playGameVsEngine(this->engine.isPlayerWhite);
        }

        return true;
    }

    void Driver::saveGame() {
    /*    
        if (std::filesystem::exists("save.txt")) {
            char ans;
            
            std::cout << "A save file has been detected, do you want to overwrite it? (Y/N) ";
            std::cin >> ans;
            if (ans == 'Y' || ans == 'y') {
            std::filesystem::remove("saves/save.txt");
            }
            else {
                return;
            }   
        }
    
        std::ofstream SaveFile("saves/save.txt");
        SaveFile << engine.board.getCurrentFen();
        SaveFile.close();
    */
    }
}