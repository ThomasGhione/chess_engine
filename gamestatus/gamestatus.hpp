#ifndef GAMESTATUS_HPP
#define GAMESTATUS_HPP

#include "../engine/engine.hpp"
#include "../board/board.hpp"

class GameStatus {

public:
    enum NumOfPlayers : uint8_t {
        ENGINE_VS_ENGINE = 0,
        HUMAN_VS_ENGINE = 1,
        HUMAN_VS_HUMAN = 2
    };

    engine::Engine engine;
    chess::Board board;

    GameStatus(engine::Engine engine, chess::Board board, NumOfPlayers numOfPlayers);

    NumOfPlayers getNumOfPlayers() const noexcept;
    void setNumOfPlayers(NumOfPlayers numOfPlayers) noexcept;

private:
    NumOfPlayers numOfPlayers;

    bool isNumOfPlayersValid(NumOfPlayers numOfPlayers) const noexcept;
};

#endif
