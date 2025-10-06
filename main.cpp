#include "chessengine.hpp"

int main() {

    #ifdef DEBUG
        ios::sync_with_stdio(false);                   // faster cout - NEED TO TEST OUT WITH printf
        cout << "\n!!!    RUNNING DEBUG BUILD    !!!\n\n";
        auto start = chrono::steady_clock::now();
    #endif

    // TODO: this code is a mess, clean it up
    chess::gameStatus gamestatus;           // declare our gamestatus; main variable where every info about the game is here                             
    chess::startingPosition(gamestatus);    // create an empty board and put every pieces in its initial squares
    print::getBoard(gamestatus);          // print the initial position

   

    #ifdef DEBUG
        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        cout << "Time to set up the game: " << chrono::duration <double, milli> (diff).count() << " ms\n\n";
    #endif   

    unsigned char option = chess::gameStarts();             // option will be put inside the switch and return the player
                                                            // black can't start first so it has no meaning, after the engine is ready it'll have a meaning choosing to play as black 
    switch (option) {                                       // TODO - this switch is a placeholder
        case 'W':
            //gamestatus.player = WHITE;
            break;
        case 'B':
            //gamestatus.player = BLACK;
            break;
        default:
            throw invalid_argument("not implemented yet");
    }

    while (true) {                                          // TODO remove placeholder => it has to be while (!checkmate()) { ... }
        inputMove(gamestatus);                              // ask the move until the game finishes
    }

    return 0;
}