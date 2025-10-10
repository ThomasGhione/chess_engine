#ifndef MENU_HPP
#define MENU_HPP

#include <iostream>
#include "../engine/engine.hpp"

namespace print {
    
    class Menu {

        public:
            static void showMainMenu(engine::Engine& engine);
            static void showOnePlayerMenu(engine::Engine& engine);
            static void showTwoPlayersMenu(engine::Engine& engine);
            static void QuitGame();

        private:
            static void clearScreen();
    };

}

#endif