#include "chessengine.h"

namespace chess {

    string playerString(const player &player) noexcept {
        return (player == WHITE) ? "WHITE" : "BLACK";
    }

    int ctoi(const char file) noexcept {
        return tolower(file) - 'a' + 1;
    }

    string getPiece(piece_id piece) noexcept {
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
            default:                return " ";
        }
    }



    //! LIST functions
    void printAllMoves(const vector<move> &moves) {}
    void printPiecesCoords(const vector<move> &moves) {}
    bool updateLegalMoves() { return false; }
    coords movePiece(unordered_set<coords> &piecesPosition, move &move) { return { 0, 0}; }

    //! VECTOR functions
}