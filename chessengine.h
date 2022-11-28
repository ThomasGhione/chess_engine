#ifndef CHESSENGINE_H
#define CHESSENGINE_H



#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <stdexcept>
#include <cctype>

//define values
#define P 1 //pawn
#define N 3
#define B 3
#define R 5
#define Q 9
#define K 1000000


// CHESS PIECE = 1 1 | 1 1 1 1 1 1
//     the first 2 bits define which player (01 meaning white, 10 meaning black)
//     the last 6 bits define which piece we're referring to
// DEFINE ID FOR PIECES:

#define EMPTY 0x00      // 00000000

#define PAWN 0x01       // 00000001    
#define KNIGHT 0x02     // 00000010
#define BISHOP 0x04     // 00000100
#define ROOK 0x08       // 00001000
#define QUEEN 0x10      // 00010000
#define KING 0x20       // 00100000
#define WHITE 0x40      // 01000000
#define BLACK 0x80      // 10000000

#define PIECEMASK 0x3F  // 00111111 
#define PLAYERMASK 0xC0 // 11000000

namespace chess {
    const char ML = 8; // ML = MAX_LINE
    
    using board = struct square {
        bool isLightSquare;             // true if light square, black if not
        unsigned char piece;            // piece
    };

    using gameStatus = struct gameStatus {
        unsigned int turns;             // turns counter
        board chessboard[ML][ML];       // cb = chessboard
    };

    void createInitialBoard(board [ML][ML]);
    void startingPosition(board [ML][ML]);
    
    std::string displayPiece(board [ML][ML], int, int);
    void debugprint(gameStatus);
    //void printBoard();


    unsigned int incTurn(unsigned int &, char &);
    int fromCharToInt(char); 
    void inputMove(gameStatus &, char &);
    char gameStarts();

    // char player, board cb, int rank1, int file1, int rank2, int file2
    bool isMoveValid(char, board [ML][ML], int, int, int, int); // 1 = startPos // 2 = newPos 
    

}




#endif