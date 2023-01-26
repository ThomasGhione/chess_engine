#include "chessengine.h"

//template<int i> struct Fac { static const int result = i * Fac<i-1>::result; };
//template<> struct Fac<1> { static const int result = 1; };

int main() {
    //std::cout << "10! = " << Fac<10>::result << "\n";

    #ifdef DEBUG
        std::ios::sync_with_stdio(false);                   // faster cout - NEED TO TEST OUT WITH printf
        std::cout << "\n!!!    RUNNING DEBUG BUILD    !!!\n\n";
        auto start = std::chrono::steady_clock::now();
    #endif


    chess::gameStatus gamestatus;                           // declare our gamestatus; main variable where every info about the game is here                             
    chess::createInitialBoard(gamestatus.chessboard);       // create an empty board and put every pieces in its initial squares
    chess::printBoard(gamestatus);                          // print the initial position

    //gamestatus.listOfMoves.resize(0);

    // TODO START OF LIST TEST
    /*
    chess::move testMove;
    testMove.file1 = testMove.file2 = 'a'; testMove.rank1 = testMove.rank2 = 1; testMove.piece = (WHITE | ROOK);
    gamestatus.listOfMoves.push_front(testMove);
    testMove.file1 = testMove.file2 = 'b'; testMove.rank1 = testMove.rank2 = 2; testMove.piece = (WHITE | BISHOP);
    gamestatus.listOfMoves.push_front(testMove);
    testMove.file1 = testMove.file2 = 'c'; testMove.rank1 = testMove.rank2 = 3; testMove.piece = (BLACK | KNIGHT);
    gamestatus.listOfMoves.push_front(testMove);
    chess::printAllMoves(gamestatus.listOfMoves);
    */
    // TODO END OF LIST TEST

    #ifdef DEBUG
        auto end = std::chrono::steady_clock::now();
        auto diff = end - start;
        std::cout << "Time to set up the game: " << std::chrono::duration <double, std::milli> (diff).count() << " ms\n\n";
    #endif   

    unsigned char option = chess::gameStarts();             // option will be put inside the switch and return the player
                                                            // black can't start first so it has no meaning, after the engine is ready it'll have a meaning choosing to play as black 
    switch (option) {                                       // TODO - this switch is a placeholder
        case 'W': /*gamestatus.player = WHITE;*/ break;
        case 'B': /*gamestatus.player = BLACK;*/ break;
        default: throw std::invalid_argument("not implemented yet");
    }

    while (true) {                                          // TODO remove placeholder => it has to be while (!checkmate()) { ... }
        inputMove(gamestatus);                              // ask the move until the game finishes
    }

    return 0;
}