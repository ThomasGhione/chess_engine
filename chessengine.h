#ifndef CHESSENGINE_H
#define CHESSENGINE_H

#include "includes.h"
#include "defines.h"

using namespace std;

namespace chess {

    using piece_id = unsigned char;
    using player = unsigned char;

    template<typename key, typename value>
    using umap = unordered_map<key, value>;

    template<typename value>
    using uset = unordered_set<value>;

    inline static const unsigned char ML = 8;   // "ML" stands for "MAX_LINE", it's going to be our board's size (8x8 squares)

    struct coords {
        char file;
        int rank;

        coords() : file(EMPTY), rank(EMPTY) {};
        coords(char f, int r) : file(f), rank(r) {};
        // coords(coords &c) : file(c.file), rank(c.rank) {};
    };

    struct piece {
        piece_id id;
        coords coords;

        piece() : id(EMPTY), coords() {};
        piece(piece_id i, chess::coords c) : id(i), coords(c) {};
        // piece(piece &p) : id(p.id), coords(p.coords) {};
    };
    
    struct move {
        piece piece;
        coords movesTo; // coords of the square after moving the piece

        move() : piece(), movesTo() {};
        move(chess::piece p, coords m) : piece(p), movesTo(m) {};
        // move(move &m) : piece(m.piece), movesTo(m.movesTo) {};
    };


    using board = struct square {
        piece piece;          // piece
    };

    
    struct gameStatus {
        //static inline checkStruct check;             // becomes true everytime a player is in check

        player player = WHITE;    // tells which player has to move. white always starts first
        size_t turns = 0;          // counts the turn, it's set to 0 before the game starts and increment every time white moves
        vector<move> listOfMoves;  // records a game moves

        uset<coords> piecesPosition();          // TODO: keeps track of piece positions in the board
        umap<piece_id, vector<coords>> legalMoves();        // TODO: keeps track of white legal moves             // TODO: keeps track of black legal moves
        
        board chessboard[ML][ML];        // cb = chessboard
        bool hasAlreadyMoved[6] {false};   // ORDER: king e1, rook a1, rook h1, king e8, rook a8, rook h8. specific values become true after one of them moves
        
        move lastMove;
    };


    // setting up
    void startingPosition(gameStatus &) noexcept;
    
    // printing
    string printPiece(const board [ML][ML], const int, const int) noexcept ;
    void printBoard(gameStatus &) noexcept;
    //void printBoard(); 


    string playerString(const unsigned char &) noexcept;
    int ctoi(const char) noexcept; 
    string getPiece(piece_id) noexcept;

    void printAllMoves(const vector<move> &);
    void printPiecesCoords(const vector<move> &);
    bool updateLegalMoves();
    coords movePiece(unordered_set<coords> &, move &);

    // game
    void inputMove(gameStatus &) noexcept;
    char gameStarts() noexcept;

    // pieces logic
    bool promotePawn(board [ML][ML], const unsigned char &, const int &, const int &) noexcept;
    bool rookMove(gameStatus &, const int &, const int &, const int &, const int &, bool) noexcept;
    bool bishopMove(board [ML][ML], const int &, const int &, const int &, const int &) noexcept;
    bool isMoveValid(gameStatus &, int, int, int, int) noexcept;

}




#endif // CHESSENGINE_H