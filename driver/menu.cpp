#include "driver.hpp"

namespace driver {

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
    // 3 -> Load Game
    // 4 -> Extra modes
    // 5 -> Quit Game
    uint32_t Driver::mainMenu() noexcept {
        clearScreen();

        static const std::string line = "\n\n==================== MAIN MENU ====================\n\n1. One Player\n2. Two Players\n3. Load Game\n4. Extra Modes\n5. Quit Game\n\nSelect an option (1-5): ";
        const uint8_t choice = readMenuChoice(line, '1', '5');

        clearScreen();
        return choice;
    }

    // It can return:
    // 1 -> Bot vs Bot (Two instances of this engine)
    // 2 -> UCI Mode
    // 3 -> Back to Main Menu
    uint32_t Driver::extraMenu() noexcept {
        clearScreen();

        static const std::string prompt = "\n\n==================== EXTRA MODES MENU ====================\n\n1. Bot Vs Bot\n2. UCI Mode\n3. Go back\n\nSelect an option (1-3): ";
        const uint8_t choice = readMenuChoice(prompt, '1', '3');

        clearScreen();
        return choice;
    }

    // It can return:
    // 1 -> Play as White
    // 2 -> Play as Black
    // 3 -> Back to Main Menu
    uint32_t Driver::playWithEngineMenu() noexcept {
        static const std::string prompt = "\n\n==================== ONE PLAYER MENU ====================\n\n1. Play as White\n2. Play as Black\n3. Back to Main Menu\n\nSelect an option (1-3): ";
        const uint8_t choice = readMenuChoice(prompt, '1', '3');

        clearScreen();
        return choice;
    }

    void Driver::clearScreen() noexcept { //! MIGHT NOT BE NOEXCEPT
#ifdef _WIN32
        [[maybe_unused]] const int result = std::system("cls");
#else
        [[maybe_unused]] const int result = std::system("clear");
#endif
    }
}
