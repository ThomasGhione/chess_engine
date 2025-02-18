#include "chessengine.hpp"

namespace chess {

    string printPlayer(const player &player) noexcept {
        return (player == WHITE) ? "WHITE" : "BLACK";
    }

    string printPiece(piece_id piece) noexcept {
        switch (piece) {     
            case (WHITE | PAWN):    return "P";
            case (WHITE | KNIGHT):  return "N";
            case (WHITE | BISHOP):  return "B";
            case (WHITE | ROOK):    return "R";
            case (WHITE | QUEEN):   return "Q";
            case (WHITE | KING):    return "K"; //TODO: \u2654
            case (BLACK | PAWN):    return "p";
            case (BLACK | KNIGHT):  return "n";
            case (BLACK | BISHOP):  return "b";
            case (BLACK | ROOK):    return "r";
            case (BLACK | QUEEN):   return "q";
            case (BLACK | KING):    return "k";
            default:                return "?";
        }
    }

    string printPieceOrEmpty(const board debugboard[ML][ML], const int rank, const int file) noexcept {
        if (debugboard[rank][file].piece.id != EMPTY)
            return printPiece(debugboard[rank][file].piece.id);
        return ((rank + file) % 2) ? "█" : " "; // default: case (EMPTY)
    }

    void printBoard(gameStatus &gs) noexcept { // white square unicode: \u2588

        #ifdef DEBUG
            auto start = chrono::steady_clock::now();
        #endif

        cout << "\n\n       A    B    C    D    E    F    G    H      LAST MOVE: "
             << printPiece(gs.lastMove.id) << ' ' << gs.lastMove.file << gs.lastMove.rank << ' ' << gs.lastMove.movesTo.file << gs.lastMove.movesTo.rank << "\n\n\n";
        for (int rank = ML - 1; rank >= 0; --rank) {
            if (rank % 2 == 1) {
                cout << "     █████     █████     █████     █████     \n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2)
                    cout << "██" << printPieceOrEmpty(gs.chessboard, rank, file) << "██  " << printPieceOrEmpty(gs.chessboard, rank, file+1) << "  ";
                cout << "    " << rank + 1 << "\n     █████     █████     █████     █████     \n";
            }
            else {
                cout << "          █████     █████     █████     █████\n" << rank + 1 << "    ";
                for (int file = 0; file < ML; file += 2)
                    cout << "  " << printPieceOrEmpty(gs.chessboard, rank, file) << "  ██" << printPieceOrEmpty(gs.chessboard, rank, file+1) << "██";
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