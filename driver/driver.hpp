#ifndef DRIVER_HPP
#define DRIVER_HPP

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"


namespace driver {

    class Driver {

        public:
            constexpr static int32_t MAX_PARAM_LENGTH = 3;
            constexpr static int32_t MODE = 1;
            constexpr static int32_t COLOR = 2;
            constexpr static int32_t NO_ARGS = 1;

            print::Menu menu;
            engine::Engine engine;

            Driver(print::Menu menu, engine::Engine engine);

            void startGame(int argc, char *argv[]) noexcept;

        private:
            void parse(int argc, char *argv[]) noexcept;

            bool loadGame(bool isWithPlayer) noexcept;
            void saveGame() noexcept;
            void endGame() noexcept;
            
            void quit(std::string input) noexcept;
            
            void playGameVsHuman() noexcept;
            void playGameVsEngine(bool isWhite) noexcept;
            void botVsBot() noexcept;
            
            void playerTurn() noexcept;
            void engineTurn() noexcept;
    };
}

#endif
