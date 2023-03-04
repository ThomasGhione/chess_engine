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
    chess::createInitialBoard(gamestatus);                  // create an empty board and put every pieces in its initial squares
    chess::printBoard(gamestatus);                          // print the initial position

    // in the starting position, these are the coordinates of where the pieces are  
    for (char c = 'a'; c <= 'h'; ++c) gamestatus.wherePieceAt.push_back({c, 1});
    for (char c = 'a'; c <= 'h'; ++c) gamestatus.wherePieceAt.push_back({c, 2});
    for (char c = 'a'; c <= 'h'; ++c) gamestatus.wherePieceAt.push_back({c, 7});
    for (char c = 'a'; c <= 'h'; ++c) gamestatus.wherePieceAt.push_back({c, 8});
    //TODO: printPieceCoordsV is just to debug
    //chess::printPieceCoordsV(gamestatus.wherePieceAt);

    /* gamestatus.wherePieceAt = { {'a', 1}, {'b', 1}, {'c', 1}, {'d', 1}, {'e', 1}, {'f', 1}, {'g', 1}, {'h', 1},
                                {'a', 2}, {'b', 2}, {'c', 2}, {'d', 2}, {'e', 2}, {'f', 2}, {'g', 2}, {'h', 2},
                                {'a', 7}, {'b', 7}, {'c', 7}, {'d', 7}, {'e', 7}, {'f', 7}, {'g', 7}, {'h', 7},
                                {'a', 8}, {'b', 8}, {'c', 8}, {'d', 8}, {'e', 8}, {'f', 8}, {'g', 8}, {'h', 8} };*/
    //?DEBUG chess::printPieceCoords(gamestatus.wherePieceAt);

    // TODO START OF LIST TEST
    /*
    chess::coords test;
    test.file = 'a'; test.rank = 1;
    gamestatus.wherePieceAt.push_back(test);
    test.file = 'b'; test.rank = 2;
    gamestatus.wherePieceAt.push_back(test);
    for (auto i : gamestatus.wherePieceAt) std::cout << i.file << i.rank;
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