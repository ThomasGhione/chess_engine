#include "chessengine.h"

int main() {
    std::ios::sync_with_stdio(false); // faster cout

    chess::gameStatus gs;
    gs.turns = 1;
    
    //chess::board chessboard[chess::ML][chess::ML];


    chess::createInitialBoard(gs.cb);
    chess::debugprint(gs.cb);


    char p = chess::gameStarts();
    while (true) {
        inputMove(gs.cb, p);
        ++gs.turns;
    }



    return 0;
}
