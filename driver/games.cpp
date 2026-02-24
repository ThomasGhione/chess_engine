#include "driver.hpp"

namespace driver {

    bool Driver::playOneTurn(bool playerTurn) noexcept {
        if (playerTurn) this->playerTurn();
        else this->engineTurn();

        if (!this->engine.isGameOver()) return false;
        this->endGame();
        return true;
    }

    void Driver::playAlternatingTurns(bool firstPlayerTurn, bool secondPlayerTurn, bool printBoard) noexcept {
        if (printBoard) {
            std::cout << print::Prints::getBasicBoard(this->engine.board) << "\n";
        }

        while (!this->engine.isGameOver()) {
            if (this->playOneTurn(firstPlayerTurn)) return;
            if (printBoard) {
                std::cout << print::Prints::getBasicBoard(this->engine.board) << "\n";
            }

            if (this->playOneTurn(secondPlayerTurn)) return;
            if (printBoard) {
                std::cout << print::Prints::getBasicBoard(this->engine.board) << "\n";
            }
        }
    }

    void Driver::playGameVsHuman() noexcept {
        vsBot = false;
        this->playAlternatingTurns(true, true, false);
    }

    void Driver::playGameVsEngine(bool isFirstTurnOfPlayer) noexcept{
        vsBot = true;
        this->playAlternatingTurns(isFirstTurnOfPlayer, !isFirstTurnOfPlayer, false);
    }

    void Driver::botVsBot() noexcept {
        this->playAlternatingTurns(false, false, true);
    }

} // namespace driver
