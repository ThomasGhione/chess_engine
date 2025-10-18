#ifndef MENU_HPP
#define MENU_HPP

#include <iostream>
#include <string>
#include <cstdint>

namespace print {

    class Menu {
      	public:
        	static u_int8_t mainMenu();
        	static u_int8_t playWithEngineMenu();
      
      	private:
        	static void clearScreen();
    };

}

#endif
