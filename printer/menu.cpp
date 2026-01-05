#include "menu.hpp"

namespace print {

    // It can return:
    // 1 -> One Player
    // 2 -> Two Players
    // 3 -> Bot Vs Bot
    // 4 -> Quit Game
    uint32_t Menu::mainMenu() noexcept{

        clearScreen();

        static const std::string line = "\n\n==================== MAIN MENU ====================\n\n1. One Player\n2. Two Players\n3. Bot Vs Bot\n4. Bot Vs Stockfish\n5. Quit Game\n\nSelect an option (1-5): ";

        uint8_t choice;

        std::cout << line;
        std::cin >> choice;

        while (choice < '1' || choice > '5') {
            std::cout << "Invalid option. Please select a valid option (1-5): ";
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
    uint32_t Menu::playWithEngineMenu() noexcept {

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
    uint32_t Menu::playWithPlayerMenu() noexcept {

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

    // It can return:
    // 1 -> Bot plays as White
    // 2 -> Bot plays as Black
    // 3 -> Back to main menu
    uint32_t Menu::playBotVsStockfishMenu() noexcept {
        static const std::string prompt = "\n\n==================== BOT VS STOCKFISH MENU ====================\n\n1. Bot plays as White\n2. Bot plays as Black\n3. Back to Main Menu\n\nSelect an option (1-3): ";

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

    void Menu::clearScreen() noexcept{
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }
}
