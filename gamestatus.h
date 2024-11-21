#ifndef CHESSENGINE_H
#define CHESSENGINE_H




#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <cctype>
#include <bitset>
#include <list>
#include <vector>
#include <algorithm>


#ifdef DEBUG
    #include <chrono>
#endif

/*
define values
#define P 100 //pawn
#define N 270
#define B 310
#define R 500
#define Q 900
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

    using piece = unsigned char;
    using LightSquare = bool;

    static inline const unsigned char DIM = 8;          // "DIM" means "MAX_LINE", it's going to be our board's size (8x8 squares)

    struct coords {
        char file;
        int rank;
    };

    struct move {
        chess::piece piece;                  // last piece moved
        char file1 = '0'; int rank1 = 0; // coords of the square before moving the piece
        char file2 = '0'; int rank2 = 0; // coords of the square after moving the piece

        move() {}
        move(chess::piece p, char f, int r) {
            this->piece = p, this->file1 = f, this->rank1 = r;
        }
        move(chess::piece p, char f1, int r1, char f2, int r2) {
            this->piece = p, this->file1 = f1, this->rank1 = r1, this->file2 = f2, this->rank2 = r2;
        }
    };


    using board = struct square {
        LightSquare isLightSquare;      // true if light square, black if not
        piece piece;                  // piece
    };

    using PiecesAndTheirLegalMoves = std::vector<std::list<move>>;


    struct gameStatus {
        //static inline checkStruct check;             // becomes true everytime a player is in check

        board chessboard[DIM][DIM];        // cb = chessboard
        bool hasAlreadyMoved[6] = {0};   // ORDER: king e1, rook a1, rook h1, king e8, rook a8, rook h8. specific values become true after one of them moves
        move lastMove;

        unsigned char player = WHITE;    // tells which player has to move. white always starts first
        unsigned int turns = 0;          // counts the turn, it's set to 0 before the game starts and increment every time white moves
        
        PiecesAndTheirLegalMoves whitePieces;             // TODO: keeps track of white pieces and their legal moves
        PiecesAndTheirLegalMoves blackPieces;             // TODO: keeps track of black pieces and their legal moves
    };

    /*
     !  the first element of the list is the piece's current position
     !  starting by the second element of the list we'll have the legal moves
     */



    //! SET UP
    void createInitialBoard(gameStatus &) noexcept;
    void startingPosition(gameStatus &) noexcept;
    
    //! PRINT PIECES AND BOARD
    std::string printPiece(const board [DIM][DIM], const int, const int) noexcept ;
    void printBoard(gameStatus &) noexcept;
    //void printBoard(); 

    //! 
    std::string playerString(const unsigned char &) noexcept;
    int fromCharToInt(const char) noexcept; 
    std::string getPiece(piece) noexcept;

    void inputMove(gameStatus &) noexcept;
    char gameStarts() noexcept;

    //! MOVE LOGIC
    bool promotePawn(board [DIM][DIM], const unsigned char &, const int &, const int &) noexcept;
    bool rookMove(gameStatus &, const int &, const int &, const int &, const int &, bool) noexcept;
    bool bishopMove(board [DIM][DIM], const int &, const int &, const int &, const int &) noexcept;
    bool isMoveValid(gameStatus &, int, int, int, int) noexcept;


    //! TRACKER
    void printPiecesCoords(const PiecesAndTheirLegalMoves&);
    void printAllPieces(const gameStatus&);
    void updatePieceCoords(PiecesAndTheirLegalMoves&, const move&);
    void deletePiece(PiecesAndTheirLegalMoves&, piece);
    void addPiece(PiecesAndTheirLegalMoves&, move);
}




#endif