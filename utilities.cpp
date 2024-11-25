#include "chessengine.h"

namespace chess {

    int ctoi(const char &file) noexcept {
        return tolower(file) - 'a' + 1;
    }

    char itoc(const int &file) noexcept {
        return file + 'a';
    }



    //! LIST functions
    /*
    void printAllMoves(const vector<move> &moves) {}
    void printPieceOrEmptysCoords(const vector<move> &moves) {}
    bool updateLegalMoves() { return false; }
    coords movePiece(unordered_set<coords> &piecesPosition, move &move) { return { 0, 0}; }
    */
    //! VECTOR functions
}