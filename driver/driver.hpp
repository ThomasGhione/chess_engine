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

            void startGame();
            void engineTurn();
        private:
            bool loadGame(bool isWithPlayer);
            void saveGame();
            void quit(std::string input);
            void endGame();

            void playerTurn();
            void playGameVsHuman();
            void playGameVsEngine(bool isWhite);
            void botVsBot();
            void EngineFirst();
            void HumanFirst();
    };
}

#endif
