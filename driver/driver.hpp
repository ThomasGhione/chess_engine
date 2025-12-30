#ifndef DRIVER_HPP
#define DRIVER_HPP

#include "../printer/menu.hpp"
#include "../engine/engine.hpp"


namespace driver {

    class Driver {

        public:
            print::Menu menu;
            engine::Engine engine;

            Driver(print::Menu menu, engine::Engine engine);

            void startGame() noexcept;

        private:
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
