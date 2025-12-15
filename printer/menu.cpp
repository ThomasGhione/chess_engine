#include "menu.hpp"

namespace print {

    // It can return:
    // 1 -> One Player
    // 2 -> Two Players
    // 3 -> Quit Game
    uint8_t Menu::mainMenu() {

        clearScreen();

        static const std::string line = "\n\n==================== MAIN MENU ====================\n\n1. One Player\n2. Two Players\n3. Quit Game\n\nSelect an option (1-3): ";

        uint8_t choice;

        std::cout << line;
        std::cin >> choice;

        while (choice < '1' || choice > '3') {
            std::cout << "Invalid option. Please select a valid option (1-3): ";
            std::cin >> choice;
        }

        clearScreen();
                
        return choice;
    }

    // It can return:
    // 1 -> Play as White
    // 2 -> Play as Black
    // 3 -> Load Game
    // 4 -> Back to Main Menu
    uint8_t Menu::playWithEngineMenu() {

        static const std::string prompt = "\n\n==================== ONE PLAYER MENU ====================\n\n1. Play as White\n2. Play as Black\n3. Load Game\n4. Back to Main Menu\n\nSelect an option (1-4): ";

        std::cout << prompt;

        uint8_t choice;
        std::cin >> choice;

        while (choice < '1' || choice > '4') {
            std::cout << "Invalid option. Please select a valid option (1-4): ";
            std::cin >> choice;
        }

        clearScreen();
        return choice;
    }

    // It can return:
    // 1 -> New game
    // 2 -> Load game
    // 3 -> Back to main menu
    uint8_t Menu::playWithPlayerMenu() {

        static const std::string prompt = "\n\n==================== TWO PLAYERS MENU ====================\n\n1. New Game\n2. Load Game\n3. Back to Main Menu\n\nSelect an option (1-3): ";

        std::cout << prompt;

        uint8_t choice;
        std::cin >> choice;

        while (choice < '1' || choice > '3') {
            std::cout << "Invalid option. Please select a valid option (1-3): ";
            std::cin >> choice;
        }

        clearScreen();
        return choice;
    }

    void Menu::clearScreen() {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }
}
