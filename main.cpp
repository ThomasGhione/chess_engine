#include "./engine/engine.hpp"
#include "./driver/driver.hpp"
#include "./uci/uci.hpp"

using namespace chess;
using namespace engine;
using namespace driver;

#ifndef _WIN32
#include <unistd.h>   // isatty, STDIN_FILENO
#else
#include <io.h>        // _isatty, _fileno
#endif

// ./chess -> Goes to main menu
// ./chess uci -> UCI mode (used by lichess-bot / GUIs)
// ./chess bvb -> Bot vs Bot
// ./chess pvp -> Player vs Player
// ./chess pvb w -> Player vs Bot (player is white)
// ./chess pvb b -> Player vs Bot (player is black)
// ./chess bvs w -> Bot vs Stockfish (bot is white)
// ./chess bvs b -> Bot vs Stockfish (bot is black)
// ./chess 11 -> Option 1 (player vs bot) - Option 1 (white)
// ./chess 12 -> Option 1 (player vs bot) - Option 2 (black)

static bool stdinIsPipe() noexcept {
#ifndef _WIN32
    return !isatty(STDIN_FILENO);
#else
    return !_isatty(_fileno(stdin));
#endif
}

int main(int argc, char *argv[]) {
    // UCI auto-detection: if stdin is a pipe (launched by a GUI / lichess-bot),
    // enter UCI mode immediately — this is the standard UCI protocol behaviour.
    if (argc == 1 && stdinIsPipe()) {
        Engine engine = Engine();
        uci::UCI uciInterface(engine);
        uciInterface.mainLoop();
        // mainLoop is [[noreturn]]
    }

    Engine engine = Engine();

    Driver driver = Driver(engine);
    driver.startGame(argc, argv);
    return 0;
}
