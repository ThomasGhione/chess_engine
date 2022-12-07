#include "chessengine.h"

int main() {
    std::ios::sync_with_stdio(false); // faster cout

    chess::gameStatus gamestatus;
    gamestatus.turns = 0;

    chess::createInitialBoard(gamestatus.chessboard);
    chess::debugprint(gamestatus);


    unsigned char p = chess::gameStarts();

    switch (p) {
        case 'W': p = WHITE; break;
        case 'B': p = BLACK; break;
    }

    while (true) {
        inputMove(gamestatus, p); //return 0;
    }

    return 0;
}