#include "chessengine.h"

namespace chess {

    void startingPosition(gameStatus &gs) noexcept {
        
        // white pieces
        gs.chessboard[0][0].piece = { { 'a', 1 }, WHITE | ROOK };
        gs.chessboard[0][1].piece = { { 'b', 1 }, WHITE | KNIGHT };
        gs.chessboard[0][2].piece = { { 'c', 1 }, WHITE | BISHOP };
        gs.chessboard[0][3].piece = { { 'd', 1 }, WHITE | QUEEN };
        gs.chessboard[0][4].piece = { { 'e', 1 }, WHITE | KING };
        gs.chessboard[0][5].piece = { { 'f', 1 }, WHITE | BISHOP };
        gs.chessboard[0][6].piece = { { 'g', 1 }, WHITE | KNIGHT };
        gs.chessboard[0][7].piece = { { 'h', 1 }, WHITE | ROOK };

        // black pieces
        gs.chessboard[7][0].piece = { { 'a', 8 }, BLACK | ROOK };
        gs.chessboard[7][1].piece = { { 'b', 8 }, BLACK | KNIGHT };
        gs.chessboard[7][2].piece = { { 'c', 8 }, BLACK | BISHOP };
        gs.chessboard[7][3].piece = { { 'd', 8 }, BLACK | QUEEN };
        gs.chessboard[7][4].piece = { { 'e', 8 }, BLACK | KING };
        gs.chessboard[7][5].piece = { { 'f', 8 }, BLACK | BISHOP };
        gs.chessboard[7][6].piece = { { 'g', 8 }, BLACK | KNIGHT };
        gs.chessboard[7][7].piece = { { 'h', 8 }, BLACK | ROOK };

        // all pawns
        for (int file = 0; file < ML; ++file) {
            gs.chessboard[1][file].piece = { { itoc(file), 2 }, WHITE | PAWN };
            gs.chessboard[6][file].piece = { { itoc(file), 7 }, BLACK | PAWN };
        }

        // empty squares
        for (int rank = 2; rank < 6; ++rank)
            for (int file = 0; file < ML; ++file)
                gs.chessboard[rank][file].piece = { { itoc(file), rank + 1}, EMPTY };
    } 

    //start of the game
    char gameStarts() noexcept {
        cout << "MENU:\n    W: play as white\n    B: play as black\n    S: save (to add)\n    L: load last game (to add)\n    Q: quit\nINPUT: ";
        char option;
        cin >> option;

        if (option == 'q' || option == 'Q') // quits
            exit(EXIT_SUCCESS);

        return toupper(option);
    }

}