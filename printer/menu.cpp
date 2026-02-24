#include "menu.hpp"
#include <iostream>
#include <string>

namespace print {

    static uint8_t readMenuChoice(const std::string& prompt, uint8_t minChoice, uint8_t maxChoice) noexcept {
        std::cout << prompt;

        uint8_t choice;
        std::cin >> choice;

        while (choice < minChoice || choice > maxChoice) {
            std::cout << "Invalid option. Please select a valid option (" << minChoice << "-" << maxChoice << "): ";
            std::cin >> choice;
        }
        return choice;
    }

    // It can return:
    // 1 -> One Player
    // 2 -> Two Players
    // 3 -> Bot Vs Bot
    // 4 -> Bot Vs Stockfish
    // 5 -> Load Game
    // 6 -> Quit Game
    uint32_t Menu::mainMenu() noexcept{
        clearScreen();

        static const std::string line = "\n\n==================== MAIN MENU ====================\n\n1. One Player\n2. Two Players\n3. Load Game\n4. Extra Modes\n5. Quit Game\n\nSelect an option (1-5): ";
        const uint8_t choice = readMenuChoice(line, '1', '5');

        clearScreen();
        return choice;
    }

    uint32_t Menu::extraMenu() noexcept{
        clearScreen();

        static const std::string prompt = "\n\n==================== EXTRA MODES MENU ====================\n\n1. Bot Vs Bot\n2. Bot Vs Stockfish\n3. Beta vs Alpha Engines\n4. UCI Mode\n5. Go back\n\nSelect an option (1-5): ";
        const uint8_t choice = readMenuChoice(prompt, '1', '5');

        clearScreen();
        return choice;
    }

    // It can return:
    // 1 -> Play as White
    // 2 -> Play as Black
    // 3 -> Back to Main Menu
    uint32_t Menu::playWithEngineMenu() noexcept {
        static const std::string prompt = "\n\n==================== ONE PLAYER MENU ====================\n\n1. Play as White\n2. Play as Black\n3. Back to Main Menu\n\nSelect an option (1-3): ";
        const uint8_t choice = readMenuChoice(prompt, '1', '3');

        clearScreen();
        return choice;
    }

    // It goes straight to a new game
    uint32_t Menu::playWithPlayerMenu() noexcept {

        /*
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
        */

        // Always starts a new game, this function needs to be changed later when multiple loaded games are supported
        return 1; 
    }

    // It can return:
    // 1 -> Bot plays as White
    // 2 -> Bot plays as Black
    // 3 -> Back to main menu
    uint32_t Menu::playBotVsStockfishMenu() noexcept {
        static const std::string prompt = "\n\n==================== BOT VS STOCKFISH MENU ====================\n\n1. Bot plays as White\n2. Bot plays as Black\n3. Back to Main Menu\n\nSelect an option (1-3): ";
        const uint8_t choice = readMenuChoice(prompt, '1', '3');

        clearScreen();
        return choice;
    }

    uint32_t Menu::playBetaVsAlphaMenu() noexcept {
        static const std::string prompt = "\n\n==================== BETA VS ALPHA MENU ====================\n\n1. Beta is White\n2. Beta is Black\n3. Back to Main Menu\n\nSelect an option (1-3): ";
        const uint8_t choice = readMenuChoice(prompt, '1', '3');

        clearScreen();
        return choice;
    }

    void Menu::clearScreen() noexcept { //! MIGHT NOT BE NOEXCEPT
#ifdef _WIN32
        [[maybe_unused]] const int result = std::system("cls");
#else
        [[maybe_unused]] const int result = std::system("clear");
#endif
    }
}
