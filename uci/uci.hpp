#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <string_view>

namespace engine {
    class Engine;
}

namespace uci {

class UCI {

public:

    UCI(engine::Engine& engine);
    ~UCI() noexcept;

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

    std::thread searchThread;
    std::mutex searchMutex;
    std::string searchBestMove = "0000";
    bool searchPonder = false;
    bool searchDone = false;
    bool searchPrinted = true;

    void finishSearch(bool requestStop, bool printBestMove) noexcept;
    void emitBestMove(std::string_view move) noexcept; // caller must hold searchMutex
    void parseMoves(std::string_view moves) noexcept;
    void parseFEN(std::string_view fen) noexcept;

}; // class UCI

}
