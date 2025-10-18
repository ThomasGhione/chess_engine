#include "menu.hpp"

namespace print {

    // It can return:
    // 1 -> One Player
    // 2 -> Two Players
    // 0 -> Quit Game
    uint8_t Menu::mainMenu() {

        static const std::string line = "\n\n==================== MAIN MENU ====================\n\n1. One Player\n2. Two Players\n3. Quit Game\n\nSelect an option (1-3): ";

        uint8_t choice;

        std::cout << line;
        std::cin >> choice;

        while (choice < 1 || choice > 3) {
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

        while (choice < 1 || choice > 4) {
            std::cout << "Invalid option. Please select a valid option (1-4): ";
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


/*


switch (choice) {
    case '1':
        showOnePlayerMenu(engine);
        break;
    case '2':
        showTwoPlayersMenu(engine);
        break;
    case '3':
        QuitGame();
        break;
}
 
 */
/*void Menu::showOnePlayerMenu(engine::Engine& engine) {

        clearScreen();
        
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
                // Back to main menu
                // showMainMenu(engine);
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
                // Back to main menu
                // showMainMenu(engine);
                break;
        }
    }

    void Menu::QuitGame() {
        clearScreen();
        
        std::cout << "\nThank you for playing! Goodbye!\n";

        exit(EXIT_SUCCESS);
    }


*/
