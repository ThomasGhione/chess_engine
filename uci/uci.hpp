#ifndef UCI_HPP
#define UCI_HPP

#include <string>
#include <iostream>

namespace uci {

    class UCI final {

        public:
            UCI();

            void mainLoop() noexcept;

            // Parser
            void parseCommand(const std::string& command) noexcept;

            // Standard UCI commands
            void quit() noexcept;
            void uci() noexcept;
            void setOption() noexcept;
            void position() noexcept;
            void ucinewgame() noexcept;
            void isready() noexcept;
            void go() noexcept;
            void stop() noexcept;
            void ponderhit() noexcept;

        private:

    }; // class UCI

}


#endif