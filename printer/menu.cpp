#include "menu.hpp"

namespace print {
  std::string Menu::getMainMenu() {
    const std::string line1 ="\n\n==================== MAIN MENU ====================\n\n";
    const std::string line2 ="1. One Player\n";
    const std::string line3 ="2. Two Players\n";
    const std::string line4 ="0. Quit Game\n\n";
    const std::string line5 ="Select an option (1-3): ";
    return line1 + line2 + line3 + line4 + line5;
  }

  uint8_t Menu::getPlayerInput(){
    char choice;
    std::cin >> choice;
    while (choice < '0' || choice > '2') {
        std::cout << "Invalid option. Please select a valid option (1-3): ";
        std::cin >> choice;
    }

    return static_cast<uint8_t>(choice);
  }

  std::string Menu::getPlayWhitEngineMenu(){

    const std::string line1= "\n\n==================== ONE PLAYER MENU ====================\n\n";
    const std::string line2= "1. Play as White\n";
    const std::string line3= "2. Play as Black\n";
    const std::string line4= "3. Load Game\n";
    const std::string line5= "4. Back to Main Menu\n\n";

    return line1 + line2 + line3 + line4 + line5;
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

    void Menu::clearScreen() {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }*/
