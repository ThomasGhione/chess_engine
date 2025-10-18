#ifndef DRIVER_HPP
#define DRIVER_HPP

#include "../printer/prints.hpp"
#include "../engine/engine.hpp"


namespace driver {

    class Driver {

        public:
            Driver(menu::Menu menu, engine::Engine engine);

        private:
            menu::Menu menu;
            engine::Engine engine;

    }

}

#endif