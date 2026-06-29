#pragma once

#include <cstdint>
#include <string>

#include "../uci/uci.hpp"

namespace chess { class Board; }
namespace engine { class Engine; }

namespace driver {

//FIXME per errori usare cerr e non cout.
//FIXME Usare this in chiamate funzioni interne alla classe
//FIXME Evitiamo di usare le stringhe stile C.
class Driver final {

public:

    enum class GameMode : uint8_t { PvP, PvE, BvB };

    engine::Engine& engine;
    uci::UCI uciInterface;

    explicit Driver(engine::Engine& engine);

    [[noreturn]] void startGame(int argc, char* argv[]) noexcept;
    static std::string getBasicBoard(const chess::Board& board);

private:

    GameMode mode_ = GameMode::PvP;

    void parse(int argc, char* argv[]) noexcept;

    bool loadGame() noexcept;
    void saveGame() noexcept;
    void endGame() noexcept;
    void printGameOnFile() noexcept;

    void startSession(GameMode mode, bool playerIsWhite = true) noexcept;
    void playerTurn() noexcept;
    void engineTurn() noexcept;
    void playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept;

    //FIXME Ci sono tanti parametri per questa funzione
    static uint32_t showMenu(const char* prompt, uint8_t minChoice, uint8_t maxChoice, bool clearBefore = true) noexcept;
    static uint32_t mainMenu() noexcept;
    static uint32_t extraMenu() noexcept;
    static uint32_t playWithEngineMenu() noexcept;

    static void clearScreen() noexcept;
};

} // namespace driver
