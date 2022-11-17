#include "chessengine.h"

int main() {
    std::ios::sync_with_stdio(false); // faster cout

    
    chess::board chessboard[chess::ML][chess::ML];
    int turn = 0;

    chess::createInitialBoard(chessboard);
    chess::debugprint(chessboard);

    char p = chess::gameStarts(chessboard);
    while (true) {
        inputMove(chessboard, p);
        ++turn;
    }



    return 0;
}
