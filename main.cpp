#include "./engine/engine.hpp"
#include "./driver/driver.hpp"

using namespace chess;
using namespace print;
using namespace engine;
using namespace driver;

int main() {
    Menu menu = Menu();
    Engine engine = Engine();

    Driver driver = Driver(menu, engine);

    driver.startGame();
}
