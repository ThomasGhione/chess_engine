#ifndef MENU_HPP
#define MENU_HPP

#include <iostream>
#include <string>
#include <cstdint>

namespace print {

  class Menu {
    public:
      static std::string getMainMenu();
      static std::string getPlayWhitEngineMenu();

      static uint8_t getPlayerInput();
    private:
      //static void clearScreen();
  };

}

#endif
