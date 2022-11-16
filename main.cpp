#include "chessengine.h"

int main() {
    std::ios::sync_with_stdio(false);

    
    chess::board chessboard[chess::ML][chess::ML];

    chess::createInitialBoard(chessboard);
    chess::debugprint(chessboard);



    return 0;
}
