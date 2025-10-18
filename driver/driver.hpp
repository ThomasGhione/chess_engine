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
            void fromMenuToEngine();
            void fromEngineToMenu();

        private:

    };

}

#endif