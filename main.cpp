#include "chessengine.h"

int main() {
    std::ios::sync_with_stdio(false);                                   // faster cout

    chess::gameStatus gamestatus;                                       // declare our gamestatus; main variable where every info about the game is here
    gamestatus.turns = 0;                                               // set the turns to 0 before the game starts
    for (int i = 0; i < 6; ++i) gamestatus.hasAlreadyMoved[i] = false;  // rooks and kings didn't move yet so we set everything to false
    gamestatus.lastMoveArray[0].file1 = gamestatus.lastMoveArray[0].file2 = gamestatus.lastMoveArray[1].file1 = gamestatus.lastMoveArray[1].file2 = 'X';
    gamestatus.lastMoveArray[0].rank1 = gamestatus.lastMoveArray[0].rank2 = gamestatus.lastMoveArray[1].rank1 = gamestatus.lastMoveArray[1].rank2 = 0;

    chess::createInitialBoard(gamestatus.chessboard);                   // create an empty board and put every pieces in its initial squares
    chess::debugprint(gamestatus);                                      // print the initial position


    unsigned char p = chess::gameStarts();                              // p stands for player, which is gonna be WHITE at the start of the game

    switch (p) {                                                        // TODO remove placeholder
        case 'W': p = WHITE; break;
        case 'B': p = BLACK; break;
    }

    while (true) {                                                      // TODO remove placeholder => it has to be while (!checkmate()) { ... }
        inputMove(gamestatus, p); //return 0;                           // ask the move until the game finishes
    }

    return 0;
}