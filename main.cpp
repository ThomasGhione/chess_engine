#include <iostream>

#include "./engine/engine.hpp"
#include "./driver/driver.hpp"
#include "./uci/uci.hpp"

#ifndef _WIN32
#include <unistd.h> // isatty, STDIN_FILENO
#else
#include <io.h>     // _isatty, _fileno
#endif

using namespace chess;
using namespace engine;
using namespace driver;

// UCI auto-detection: if stdin is a pipe (launched by a GUI / lichess-bot),
// enter UCI mode immediately - this is the standard UCI protocol behaviour.
static bool stdinIsPipe() noexcept {
#ifndef _WIN32
    return !isatty(STDIN_FILENO);
#else
    return !_isatty(_fileno(stdin));
#endif
}

int main(int argc, char *argv[]) {

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Engine engine;

    if (argc == 1 && stdinIsPipe()) {
        uci::UCI uciInterface(engine);
        uciInterface.mainLoop(); //mainLoop() is [[noreturn]] so this will exit the program when done
    }

    Driver driver(engine);
    driver.startGame(argc, argv);
}
