#ifndef CHESSENGINE_H
#define CHESSENGINE_H



#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <stdexcept>



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

namespace chess {
    const char ML = 8; // MAX_LINE

/*
    const unsigned char pawn = PAWN;           
    const unsigned char knight = KNIGHT;         
    const unsigned char bishop = BISHOP;         
    const unsigned char rook = ROOK;           
    const unsigned char queen = QUEEN;          
    const unsigned char king = KING;  
    const unsigned char white = WHITE;          
    const unsigned char black = BLACK;          
*/

    using board = struct square {
        bool isLightSquare;
        bool isEmpty;
        unsigned char piece;
    };


    void createInitialBoard(board [ML][ML]);
    void startingPosition(board [ML][ML]);
    
    std::string displayPiece(board [ML][ML], int, int);
    void debugprint(board [ML][ML]);
    //void printBoard();

}




#endif