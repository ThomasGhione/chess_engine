#include "chessengine.h"

int main() {
    std::ios::sync_with_stdio(false); // faster cout

    chess::gameStatus gamestatus;
    gamestatus.turns = 0;

    chess::createInitialBoard(gamestatus.chessboard);
    chess::debugprint(gamestatus);


    char p = chess::gameStarts();
    while (true) {
        inputMove(gamestatus, p);
        ++gamestatus.turns;
    }

    return 0;
}
