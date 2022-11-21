#include "chessengine.h"

namespace chess {

    void startingPosition(board emptyboard[ML][ML]) {
        //pieces
        emptyboard[0][0].piece = emptyboard[0][7].piece = WHITE | ROOK;
        emptyboard[0][1].piece = emptyboard[0][6].piece = WHITE | KNIGHT;
        emptyboard[0][2].piece = emptyboard[0][5].piece = WHITE | BISHOP;
        emptyboard[0][3].piece = WHITE | QUEEN;
        emptyboard[0][4].piece = WHITE | KING;

        //black pieces
        emptyboard[7][0].piece = emptyboard[7][7].piece = BLACK | ROOK;
        emptyboard[7][1].piece = emptyboard[7][6].piece = BLACK | KNIGHT;
        emptyboard[7][2].piece = emptyboard[7][5].piece = BLACK | BISHOP;
        emptyboard[7][3].piece = BLACK | QUEEN;
        emptyboard[7][4].piece = BLACK | KING;

        //both sides' pawns
        for (int file = 0; file < ML; ++file) {
            emptyboard[1][file].piece = WHITE | PAWN;
            emptyboard[6][file].piece = BLACK | PAWN;
        }

        //empty squares
        for (int rank = 2; rank < 6; ++rank) 
            for (int file = 0; file < ML; ++file) 
                emptyboard[rank][file].piece = EMPTY;
    } 


    void createInitialBoard(board new_board[ML][ML]) {
        for (int rank = 0; rank < ML; ++rank) 
            for (int file = 0; file < ML; ++file) 
                ((rank + file) % 2 == 0) 
                ? (new_board[rank][file].isLightSquare = false) 
                : (new_board[rank][file].isLightSquare = true);
        startingPosition(new_board);
    }


    std::string displayPiece(board debugboard[ML][ML], int rank, int file) {
        switch (debugboard[rank][file].piece) { 
            case (EMPTY): return (debugboard[rank][file].isLightSquare) ? "██" : "  ";     
            case (WHITE | PAWN): return "wp";
            case (WHITE | KNIGHT): return "wN";
            case (WHITE | BISHOP): return "wB";
            case (WHITE | ROOK): return "wR";
            case (WHITE | QUEEN): return "wQ";
            case (WHITE | KING): return "wK";
            case (BLACK | PAWN): return "bp";
            case (BLACK | KNIGHT): return "bN";
            case (BLACK | BISHOP): return "bB";
            case (BLACK | ROOK): return "bR";
            case (BLACK | QUEEN): return "bQ";
            case (BLACK | KING): return "bK"; 
            default: throw std::invalid_argument("unexpected bitwise inclusive-OR");   
        }
    }

    void debugprint(gameStatus gamestatus) {
        //! white: \u2588
        std::cout << "\n\n";
        for (int rank = ML - 1; rank >= 0; --rank) {
            if (gamestatus.chessboard[rank][0].isLightSquare) {
                std::cout << "     ██████      ██████      ██████      ██████      \n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2) std::cout << "██" << displayPiece(gamestatus.chessboard, rank, file) << "██  " << displayPiece(gamestatus.chessboard, rank, file+1) << "  ";
                std::cout << "\n     ██████      ██████      ██████      ██████      \n";
            } else {
                std::cout << "           ██████      ██████      ██████      ██████\n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2) std::cout << "  " << displayPiece(gamestatus.chessboard, rank, file) << "  ██" << displayPiece(gamestatus.chessboard, rank, file+1) << "██";
                std::cout << "\n           ██████      ██████      ██████      ██████\n";
            }
        }
        std::cout << "\n\n       A     B     C     D     E     F     G     H\n\n\n";
    }

}