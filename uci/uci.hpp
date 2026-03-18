#pragma once

#include "../engine/engine.hpp"
#include <string>
#include <iostream>

namespace uci {

    class UCI {

        public:
            UCI();
            UCI(engine::Engine& engine);

            engine::Engine& engine;


            [[noreturn]] void mainLoop() noexcept;

            // Parser
            void parseCommand(const std::string& command) noexcept;

            // Standard UCI commands
            void quit() noexcept;
            void uci() noexcept;
            void setOption(const std::string& args) noexcept;
            void position(const std::string& command) noexcept;
            void ucinewgame() noexcept;
            void isready() noexcept;
            void go(const std::string& args) noexcept;
            void stop() noexcept;
            void ponderhit() noexcept;

        private:

            void parseMoves(const std::string& moves) noexcept;
            void parseFEN(const std::string& fen) noexcept;

    }; // class UCI

}


