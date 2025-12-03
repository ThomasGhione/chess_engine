#include "gamestatus.hpp"

GameStatus::GameStatus(engine::Engine engine, chess::Board board, NumOfPlayers numOfPlayers)
    : engine(engine)
    , board(board)
{
    if (!isNumOfPlayersValid(numOfPlayers)) {
        throw std::invalid_argument("Invalid number of players.");
    }
    this->numOfPlayers = numOfPlayers;
}

bool GameStatus::isNumOfPlayersValid(NumOfPlayers numOfPlayers) const noexcept {
    return (numOfPlayers == ENGINE_VS_ENGINE ||
            numOfPlayers == HUMAN_VS_ENGINE ||
            numOfPlayers == HUMAN_VS_HUMAN);
}

GameStatus::NumOfPlayers GameStatus::getNumOfPlayers() const noexcept {
    return this->numOfPlayers;
}

void GameStatus::setNumOfPlayers(NumOfPlayers numOfPlayers) noexcept {
    if (isNumOfPlayersValid(numOfPlayers)) {
        this->numOfPlayers = numOfPlayers;
    }
}