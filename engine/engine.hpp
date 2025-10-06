#include "board/board.hpp"
#include <unordered_map>
#include <cstdint>

namespace engine {

/*
    -Engine (a differenza di board) non sarà solo una "foto" della board,
        dovrà sicuramente contenere tutte le mosse legali della mossa corrente,
        ed una evaluation dell'attuale posizione
    -sarà un aggregatore dei pezzi, avrà sicuramente all'interno un elemento di tipo board
    -read/write i/o
    -gestione input
*/



class Engine {

    using PieceMovesMap = std::unordered_map<chess::Piece, std::vector<chess::coords>>;
    
public:

    Engine();
    ~Engine();

    double eval;

    void playGameVsEngine(); // ciclo prinipale
    void playGameVsHuman(); // same as above :)

    void saveGame();
    void loadGame();

private:
    chess::Board board;
    
    // PieceMovesMap legalMoves;

    PieceMovesMap getLegalMoves();

    void takePlayerTurn(); // muove il giocatore...
    void takeEngineTurn(); // ...mossa dopo muove l'engine

    double evaluate();

    bool isMate();
    bool isStalemate();
};

}