#pragma once

#include <string_view>

namespace engine {
    class Engine;
}

namespace uci {

class UCI {

public:

    UCI(engine::Engine& engine);

    engine::Engine& engine;

    [[noreturn]] void mainLoop() noexcept;

    // Parser
    void parseCommand(std::string_view command) noexcept;

    // Standard UCI commands
    void quit() noexcept;
    void uci() noexcept;
    void setOption(std::string_view args) noexcept;
    void position(std::string_view command) noexcept;
    void ucinewgame() noexcept;
    void isready() noexcept;
    void go(std::string_view args) noexcept;
    void stop() noexcept;
    void ponderhit() noexcept;

private:

    void parseMoves(std::string_view moves) noexcept;
    void parseFEN(std::string_view fen) noexcept;

}; // class UCI

}
