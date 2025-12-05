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
            
        private:    
            bool loadGame(bool isWithPlayer);
            void saveGame();
            void quit(char input);

            void playGameVsHuman();
            void takePlayerTurn();

            void playGameVsEngine(bool isWhite);
            void EngineFirst();
            void HumanFirst();

            void printFinalResult(); // TODO implement this!!!
    };
}

#endif
