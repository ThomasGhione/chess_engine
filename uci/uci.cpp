#include "uci.hpp"

namespace uci {
    
    UCI::UCI() {
    
    } // Constructor implementation (if needed)

    void UCI::mainLoop() noexcept {
        
        while(true) {
            std::string command;
            std::getline(std::cin, command);
            this->parseCommand(command);
        }

    }

    void UCI::parseCommand(const std::string& command) noexcept {

    }


    // Standard UCI commands
    void quit() noexcept {

    }

    void uci() noexcept {

    }

    void setOption() noexcept {

    }

    void position() noexcept {
        
    }

    void ucinewgame() noexcept {
        
    }

    void isready() noexcept {
        
    }
    
    void go() noexcept {
        
    }
    
    void stop() noexcept {
        
    }
    void ponderhit() noexcept {

    }

}