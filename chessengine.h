#ifndef CHESSENGINE_H
#define CHESSENGINE_H




#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <cctype>
#include <bitset>
#include <list>

#ifdef DEBUG
    #include <chrono>
#endif

/*
//define values
#define P 1 //pawn
#define N 3
#define B 3
#define R 5
#define Q 9
#define K 1000000
*/

/*
 *    CHESS PIECE = 1 1 | 1 1 1 1 1 1
 *                  first 2 bits define which player (01 meaning white, 10 meaning black)
 *                  last 6 bits define which piece we're referring to
 *    EXAMPLE: "1 0 | 0 0 0 1 0 0" means the piece is a black bishop
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

    static inline const unsigned char ML = 8;          // "ML" means "MAX_LINE", it's going to be our board's size (8x8 squares)

    struct move {
        static inline char file1 = '0'; static inline int rank1 = 0; // coords of the square before moving the piece
        static inline char file2 = '0'; static inline int rank2 = 0; // coords of the square after moving the piece
        static inline piece_id piece;                                // last piece moved
    };

    using board = struct square {
        light_square isLightSquare;      // true if light square, black if not
        piece_id piece;                  // piece
    };

    

    struct gameStatus {
        //static inline checkStruct check;             // becomes true everytime a player is in check

        static inline unsigned char player = WHITE;    // tells which player has to move. white always starts first
        static inline unsigned int turns = 0;          // counts the turn, it's set to 0 before the game starts and increment every time white moves
        //listOfMoves moves;                           // TODO: moves struct
        static inline board chessboard[ML][ML];        // cb = chessboard
        static inline bool hasAlreadyMoved[6] = {0};   // ORDER: king e1, rook a1, rook h1, king e8, rook a8, rook h8. specific values become true after one of them moves
        static inline move lastMove;
    };



    void createInitialBoard(board [ML][ML]) noexcept;
    void startingPosition(board [ML][ML]) noexcept;
    
    std::string printPiece(const board [ML][ML], const int, const int) noexcept ;
    void printBoard(gameStatus &) noexcept;
    //void printBoard();

    std::string playerString(const unsigned char &) noexcept;
    int fromCharToInt(const char) noexcept; 
    std::string getPiece(piece_id) noexcept;
    
    void inputMove(gameStatus &) noexcept;
    char gameStarts() noexcept;

    bool promotePawn(board [ML][ML], const unsigned char &, const int &, const int &) noexcept;
    bool rookMove(gameStatus &, const int &, const int &, const int &, const int &, bool) noexcept;
    bool bishopMove(board [ML][ML], const int &, const int &, const int &, const int &) noexcept;
    bool isMoveValid(gameStatus &, int, int, int, int) noexcept;

}




#endif