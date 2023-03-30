#include "chessengine.h"

namespace chess {

    void startingPosition(gameStatus &gs) noexcept {
        //pieces
        gs.chessboard[0][0].piece = gs.chessboard[0][7].piece = WHITE | ROOK;
        gs.chessboard[0][1].piece = gs.chessboard[0][6].piece = WHITE | KNIGHT;
        gs.chessboard[0][2].piece = gs.chessboard[0][5].piece = WHITE | BISHOP;
        gs.chessboard[0][3].piece = WHITE | QUEEN;
        gs.chessboard[0][4].piece = WHITE | KING;

        //black pieces
        gs.chessboard[7][0].piece = gs.chessboard[7][7].piece = BLACK | ROOK;
        gs.chessboard[7][1].piece = gs.chessboard[7][6].piece = BLACK | KNIGHT;
        gs.chessboard[7][2].piece = gs.chessboard[7][5].piece = BLACK | BISHOP;
        gs.chessboard[7][3].piece = BLACK | QUEEN;
        gs.chessboard[7][4].piece = BLACK | KING;

        //all pawns
        for (int file = 0; file < ML; ++file) {
            gs.chessboard[1][file].piece = WHITE | PAWN;
            gs.chessboard[6][file].piece = BLACK | PAWN;
        }

        //empty squares
        for (int rank = 2; rank < 6; ++rank) for (int file = 0; file < ML; ++file) gs.chessboard[rank][file].piece = EMPTY;
    } 


    void createInitialBoard(gameStatus &gs) noexcept { // set up light squares
        for (int rank = 0; rank < ML; ++rank)
            for (int file = 0; file < ML; ++file) 
                (((rank + file) % 2)) ? (gs.chessboard[rank][file].isLightSquare = true) : (gs.chessboard[rank][file].isLightSquare = false);
        startingPosition(gs);
    }


    std::string printPiece(const board debugboard[ML][ML], const int rank, const int file) noexcept {
        if (debugboard[rank][file].piece != EMPTY) return getPiece(debugboard[rank][file].piece);
        return (debugboard[rank][file].isLightSquare) ? "█" : " "; // default: case (EMPTY)
    }

    void printBoard(gameStatus &gs) noexcept { // white square unicode: \u2588

        #ifdef DEBUG
            auto start = std::chrono::steady_clock::now();
        #endif

        std::cout << "\n\n       A    B    C    D    E    F    G    H      LAST MOVE: " << getPiece(gs.lastMove.piece) << ' ' << gs.lastMove.file1 << gs.lastMove.rank1 << ' ' <<gs.lastMove.file2 << gs.lastMove.rank2 << "\n\n\n";
        for (int rank = ML - 1; rank >= 0; --rank) {
            if (gs.chessboard[rank][0].isLightSquare) {
                std::cout << "     █████     █████     █████     █████     \n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2) std::cout << "██" << printPiece(gs.chessboard, rank, file) << "██  " << printPiece(gs.chessboard, rank, file+1) << "  ";
                std::cout << "    " << rank + 1 << "\n     █████     █████     █████     █████     \n";
            } else {
                std::cout << "          █████     █████     █████     █████\n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2) std::cout << "  " << printPiece(gs.chessboard, rank, file) << "  ██" << printPiece(gs.chessboard, rank, file+1) << "██";
                std::cout << "    " << rank + 1 << "\n          █████     █████     █████     █████\n";
            }
        } std::cout << "\n\n       A    B    C    D    E    F    G    H      TURN: " << gs.turns << "\n\n";

        #ifdef DEBUG
            auto end = std::chrono::steady_clock::now();
            auto diff = end - start;
            std::cout << "\nTime to print: " << std::chrono::duration <double, std::milli> (diff).count() << " ms\n";
        #endif
    }

}