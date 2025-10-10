#include "menu.hpp"

namespace print {
    void Menu::showMainMenu(engine::Engine* engine) {
        clearScreen();
        
        std::cout << "\n\n==================== MAIN MENU ====================\n\n";
        std::cout << "1. One Player\n";
        std::cout << "2. Two Players\n";
        std::cout << "3. Quit Game\n\n";
        std::cout << "Select an option (1-3): ";
        
        char choice;
        std::cin >> choice;
        while (choice < '1' || choice > '3') {
            std::cout << "Invalid option. Please select a valid option (1-3): ";
            std::cin >> choice;
        }

        switch (choice) {
            case '1':
                showOnePlayerMenu(*engine);
                break;
            case '2':
                showTwoPlayersMenu(*engine);
                break;
            case '3':
                QuitGame();
                break;
        }
    }

    void Menu::showOnePlayerMenu(engine::Engine& engine) {
        clearScreen();
        
        std::cout << "\n\n==================== ONE PLAYER MENU ====================\n\n";
        std::cout << "1. Play as White\n";
        std::cout << "2. Play as Black\n";
        std::cout << "3. Load Game\n";
        std::cout << "4. Back to Main Menu\n\n";

        char choice;
        std::cin >> choice;
        while (choice < '1' || choice > '4') {
            std::cout << "Invalid option. Please select a valid option (1-4): ";
            std::cin >> choice;
        }

        switch (choice) {
            case '1':
                engine.playGameVsEngine(true);
                break;
            case '2':
                engine.playGameVsEngine(false);
                break;
            case '3':
                engine.loadGame();
                break;
            case '4':
                showMainMenu(engine);
                break;
        }
    }

    void Menu::showTwoPlayersMenu(engine::Engine& engine) {
        clearScreen();
        
        std::cout << "\n\n==================== TWO PLAYERS MENU ====================\n\n";
        std::cout << "1. Start Game\n";
        std::cout << "2. Back to Main Menu\n\n";
        std::cout << "Select an option (1-2): ";

        char choice;
        std::cin >> choice;
        while (choice < '1' || choice > '2') {
            std::cout << "Invalid option. Please select a valid option (1-2): ";
            std::cin >> choice;
        }

        switch (choice) {
            case '1':
                engine.playGameVsHuman();
                break;
            case '2':
                showMainMenu(engine);
                break;
        }
    }

    void Menu::QuitGame() {
        clearScreen();
        
        std::cout << "\nThank you for playing! Goodbye!\n";

        exit(EXIT_SUCCESS);
    }

    void Menu::clearScreen() {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }
}