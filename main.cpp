#include "chessengine.h"

int main() {
    std::ios::sync_with_stdio(false);                                   // faster cout

    chess::gameStatus gamestatus;                                       // declare our gamestatus; main variable where every info about the game is here
    gamestatus.turns = 0;                                               // set the turns to 0 before the game starts
    for (int i = 0; i < 6; ++i) gamestatus.hasAlreadyMoved[i] = false;  // rooks and kings didn't move yet so we set everything to false
    gamestatus.lastMoveArray[0].file1 = gamestatus.lastMoveArray[0].file2 = gamestatus.lastMoveArray[1].file1 = gamestatus.lastMoveArray[1].file2 = '0';
    gamestatus.lastMoveArray[0].rank1 = gamestatus.lastMoveArray[0].rank2 = gamestatus.lastMoveArray[1].rank1 = gamestatus.lastMoveArray[1].rank2 = 0;
    gamestatus.player = WHITE;                                          // white always starts first

    chess::createInitialBoard(gamestatus.chessboard);                   // create an empty board and put every pieces in its initial squares
    chess::debugprint(gamestatus);                                      // print the initial position

    unsigned char option = chess::gameStarts();                         // option will be put inside the switch and return the player
                                                                        // black can't start first so it has no meaning, after the engine is ready it'll have a meaning choosing to play as black
    switch (option) {                                                   // TODO - this switch is a placeholder
        case 'W': /*gamestatus.player = WHITE;*/ break;
        case 'B': /*gamestatus.player = BLACK;*/ break;
        default: throw std::invalid_argument("not implemented yet");
    }

    while (true) {                                                      // TODO remove placeholder => it has to be while (!checkmate()) { ... }
        inputMove(gamestatus);                                          // ask the move until the game finishes
    }

    return 0;
}