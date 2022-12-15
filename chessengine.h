#ifndef CHESSENGINE_H
#define CHESSENGINE_H




#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <stdexcept>
#include <cctype>
#include <bitset>


//define values
#define P 1 //pawn
#define N 3
#define B 3
#define R 5
#define Q 9
#define K 1000000

/*
 *    CHESS PIECE = 1 1 | 1 1 1 1 1 1
 *                  first 2 bits define which player (01 meaning white, 10 meaning black)
 *                  last 6 bits define which piece we're referring to
 *    DEFINE ID FOR PIECES:
 */

#define EMPTY 0x00      // 00000000

#define PAWN 0x01       // 00000001    
#define KNIGHT 0x02     // 00000010
#define BISHOP 0x04     // 00000100
#define ROOK 0x08       // 00001000
#define QUEEN 0x10      // 00010000
#define KING 0x20       // 00100000
#define WHITE 0x40      // 01000000
#define BLACK 0x80      // 10000000

/*
 * useful for bitwise operations:
 */

#define PIECEMASK 0x3F  // 00111111 
#define PLAYERMASK 0xC0 // 11000000



namespace chess {

    using piece_id = unsigned char;
    using light_square = bool;

    const char ML = 8; // ML = MAX_LINE

    struct lastMove {
        char file1; int rank1;          // coords of the square before moving the piece
        char file2; int rank2;          // coords of the square after moving the piece
        piece_id piece;                 // last piece moved
    };

    using board = struct square {
        light_square isLightSquare;     // true if light square, black if not
        piece_id piece;                 // piece
    };

    struct gameStatus {
        unsigned char player;
        unsigned int turns;             // turns counter
        //listOfMoves moves;            // moves struct
        board chessboard[ML][ML];       // cb = chessboard
        bool hasAlreadyMoved[6];        // king e1, rook a1, rook h1, king e8, rook a8, rook h8
        lastMove lastMoveArray[2];      // lastmove[0] is for white and lastmove[1] is for black
    };


    void createInitialBoard(board [ML][ML]);
    void startingPosition(board [ML][ML]);
    
    std::string displayPiece(board [ML][ML], int, int);
    void debugprint(gameStatus &);
    //void printBoard();


    int fromCharToInt(char); 
    void inputMove(gameStatus &);
    char gameStarts();

    bool promotePawn(board [ML][ML], unsigned char &, int &, int &);
    bool rookMove(gameStatus &, int &, int &, int &, int &, bool);
    bool bishopMove(board [ML][ML], int &, int &, int &, int &);
    // char player, board cb, int rank1, int file1, int rank2, int file2
    bool isMoveValid(gameStatus &, int, int, int, int); // 1 = startPos // 2 = newPos 

}




#endif