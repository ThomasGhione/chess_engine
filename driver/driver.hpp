#pragma once

#include <cstdint>
#include <string>

#include "../uci/uci.hpp"

namespace chess { class Board; }
namespace engine { class Engine; }

namespace driver {

class Driver final {

public:

    enum class GameMode : uint8_t { PvP, PvE, BvB };

    engine::Engine& engine;
    uci::UCI uciInterface;

    explicit Driver(engine::Engine& engine);

    [[noreturn]] void startGame(int argc, char* argv[]) noexcept;
    static std::string getBasicBoard(const chess::Board& board);

private:

    void startSession(GameMode mode, bool playerIsWhite = true) noexcept;
    void playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept;

    static uint32_t showMenu(const char* prompt, uint8_t minChoice, uint8_t maxChoice, bool clearBefore = true) noexcept;
    static void clearScreen();
};

} // namespace driver
