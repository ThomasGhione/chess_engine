#ifndef MENU_HPP
#define MENU_HPP

#include <iostream>
#include <string>
#include <cstdint>

namespace print {

    class Menu {
      	public:
        	static uint32_t mainMenu() noexcept;
        	static uint32_t playWithEngineMenu() noexcept;
			static uint32_t playWithPlayerMenu() noexcept;
			static uint32_t playBotVsStockfishMenu() noexcept;
			static uint32_t playBetaVsAlphaMenu() noexcept;
      
      	private:
        	static void clearScreen() noexcept;
    };

}

#endif
