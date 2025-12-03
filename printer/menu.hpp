#ifndef MENU_HPP
#define MENU_HPP

#include <iostream>
#include <string>
#include <cstdint>

namespace print {

    class Menu {
      	public:
        	static uint8_t mainMenu();
        	static uint8_t playWithEngineMenu();
			static uint8_t playWithPlayerMenu();
      
      	private:
        	static void clearScreen();
    };

}

#endif
