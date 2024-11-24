#include "chessengine.h"

namespace chess {

    void startingPosition(gameStatus &gs) noexcept {
        //pieces
        gs.chessboard[0][0].piece.id = gs.chessboard[0][7].piece.id = WHITE | ROOK;
        gs.chessboard[0][1].piece.id = gs.chessboard[0][6].piece.id = WHITE | KNIGHT;
        gs.chessboard[0][2].piece.id = gs.chessboard[0][5].piece.id = WHITE | BISHOP;
        gs.chessboard[0][3].piece.id = WHITE | QUEEN;
        gs.chessboard[0][4].piece.id = WHITE | KING;

        //black pieces
        gs.chessboard[7][0].piece.id = gs.chessboard[7][7].piece.id = BLACK | ROOK;
        gs.chessboard[7][1].piece.id = gs.chessboard[7][6].piece.id = BLACK | KNIGHT;
        gs.chessboard[7][2].piece.id = gs.chessboard[7][5].piece.id = BLACK | BISHOP;
        gs.chessboard[7][3].piece.id = BLACK | QUEEN;
        gs.chessboard[7][4].piece.id = BLACK | KING;

        //all pawns
        for (int file = 0; file < ML; ++file) {
            gs.chessboard[1][file].piece.id = WHITE | PAWN;
            gs.chessboard[6][file].piece.id = BLACK | PAWN;
        }

        //empty squares
        for (int rank = 2; rank < 6; ++rank)
            for (int file = 0; file < ML; ++file)
                gs.chessboard[rank][file].piece.id = EMPTY;
    } 


    string printPiece(const board debugboard[ML][ML], const int rank, const int file) noexcept {
        if (debugboard[rank][file].piece.id != EMPTY)
            return getPiece(debugboard[rank][file].piece.id);
        return ((rank + file) % 2) ? "█" : " "; // default: case (EMPTY)
    }

    void printBoard(gameStatus &gs) noexcept { // white square unicode: \u2588

        #ifdef DEBUG
            auto start = chrono::steady_clock::now();
        #endif

        cout << "\n\n       A    B    C    D    E    F    G    H      LAST MOVE: "
             << getPiece(gs.lastMove.piece.id) << ' ' << gs.lastMove.piece.coords.file << gs.lastMove.piece.coords.rank << ' ' << gs.lastMove.movesTo.file << gs.lastMove.movesTo.rank << "\n\n\n";
        for (int rank = ML - 1; rank >= 0; --rank) {
            if (rank % 2 == 1) {
                cout << "     █████     █████     █████     █████     \n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2)
                    cout << "██" << printPiece(gs.chessboard, rank, file) << "██  " << printPiece(gs.chessboard, rank, file+1) << "  ";
                cout << "    " << rank + 1 << "\n     █████     █████     █████     █████     \n";
            }
            else {
                cout << "          █████     █████     █████     █████\n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2)
                    cout << "  " << printPiece(gs.chessboard, rank, file) << "  ██" << printPiece(gs.chessboard, rank, file+1) << "██";
                cout << "    " << rank + 1 << "\n          █████     █████     █████     █████\n";
            }
        }
        cout << "\n\n       A    B    C    D    E    F    G    H      TURN: " << gs.turns << "\n\n";

        #ifdef DEBUG
            auto end = chrono::steady_clock::now();
            auto diff = end - start;
            cout << "\nTime to print: " << chrono::duration <double, milli> (diff).count() << " ms\n";
        #endif
    }

}