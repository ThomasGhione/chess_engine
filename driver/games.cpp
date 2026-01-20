#include "driver.hpp"

namespace driver {
    void Driver::playGameVsHuman() noexcept {
    	vsBot = false;
        
        while(!this->engine.isGameOver()) {
    	    //! It doesn't check for loaded games, we should fix it later based on the activeColor in board
            this->playerTurn();
            if (this->engine.isGameOver()) { endGame(); return; }

            this->playerTurn();
            if (this->engine.isGameOver()) { endGame(); return; }
    	}
    }

    void Driver::playGameVsEngine(bool isFirstTurnOfPlayer) noexcept{
        vsBot = true;
        
        if (isFirstTurnOfPlayer) {
            while (!this->engine.isGameOver()) {
                this->playerTurn();
                if (this->engine.isGameOver()) { endGame(); return; }
                
                this->engineTurn();
                if (this->engine.isGameOver()) { endGame(); return; }
            }
        } 
        else {
            while (!this->engine.isGameOver()) {
                this->engineTurn();
                if (this->engine.isGameOver()) { endGame(); return; }

                this->playerTurn();
                if (engine.isGameOver()) { endGame(); return; }
            } 
        }
    }

    void Driver::botVsBot() noexcept {
        std::string currentBoard = print::Prints::getBasicBoard(engine.board);
        std::cout << currentBoard << "\n";

        while (!this->engine.isGameOver()) {
            this->engineTurn();
            if (this->engine.isGameOver()) { endGame(); return; }
            currentBoard = print::Prints::getBasicBoard(engine.board);
            std::cout << currentBoard << "\n";

            this->engineTurn();
            if (this->engine.isGameOver()) { endGame(); return; }
            currentBoard = print::Prints::getBasicBoard(engine.board);
            std::cout << currentBoard << "\n";
        }
    }
}