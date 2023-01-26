#include "chessengine.h"

namespace chess {

    std::string playerString(const unsigned char &player) noexcept {
        return (player == WHITE) ? "WHITE" : "BLACK";
    }

    int fromCharToInt(const char file) noexcept {
        return std::tolower(file) - 'a' + 1;
    }

    std::string getPiece(piece_id piece) noexcept {
        switch (piece) {     
            case (WHITE | PAWN): return "P";
            case (WHITE | KNIGHT): return "N";
            case (WHITE | BISHOP): return "B";
            case (WHITE | ROOK): return "R";
            case (WHITE | QUEEN): return "Q";
            case (WHITE | KING): return "K"; //TODO: \u2654
            case (BLACK | PAWN): return "p";
            case (BLACK | KNIGHT): return "n";
            case (BLACK | BISHOP): return "b";
            case (BLACK | ROOK): return "r";
            case (BLACK | QUEEN): return "q";
            case (BLACK | KING): return "k";
            default: return " ";
        }
    }

    void printAllMoves(const std::list<move> &l) {
        std::cout << "\nprint of all moves: \n";
        for (const move &i : l) std::cout << getPiece(i.piece) << ' ' << i.file1 << i.rank1 << ' ' << i.file2 << i.rank2 << '\n';
        std::cout << '\n';
    }


}