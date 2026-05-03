#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace chess { class Board; }
namespace engine { class Engine; }

namespace driver {

class Driver {

public:

    struct Metadata {
        std::string_view id = "1.0.0";
        std::string_view license = "MIT License";
        std::string_view name = "HydraY! 1.0.0";
        std::array<std::string_view, 3> authors = {"Thomas Ghione", "Daniele Ferretti", "Simone Tomasella"};
        std::array<std::string_view, 1> platforms = {"Linux x86_64"}; // supported platforms
        // TODO: are these really needed?
        // size_t defaultThreads = 4;
        // size_t defaultTTSizeMB = 64;
        // bool debugMode = false;
    };

    const Metadata metadata{};

    engine::Engine& engine;

    explicit Driver(engine::Engine& engine);

    [[noreturn]] void startGame(int argc, char* argv[]) noexcept;
    static std::string getBasicBoard(const chess::Board& board);

private:

    bool vsBot = false;

    void parse(int argc, char* argv[]) noexcept;
    static void printInvalidOption() noexcept;

    bool loadGame() noexcept;
    void saveGame() noexcept; // botColor: true = bot is white, false = bot is black
    void endGame() noexcept;
    void printGameOnFile() noexcept;

    void playGameVsHuman() noexcept;
    void playGameVsEngine(bool isWhite) noexcept;
    void botVsBot() noexcept;

    void playerTurn() noexcept;
    void engineTurn() noexcept;
    bool playOneTurn(bool playerTurn) noexcept;
    void playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept;

    static uint32_t showMenu(const char* prompt, uint8_t minChoice, uint8_t maxChoice, bool clearBefore = true) noexcept;
    static uint32_t mainMenu() noexcept;
    static uint32_t extraMenu() noexcept;
    static uint32_t playWithEngineMenu() noexcept;

    static void clearScreen() noexcept;
};

} // namespace driver
