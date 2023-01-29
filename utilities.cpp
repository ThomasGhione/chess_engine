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

    void printPieceCoords(const std::list<coords> &l) {
        std::cout << "\nprint of all moves: \n";
        for (const coords &i : l) std::cout << i.file << i.rank << '\n';
        std::cout << '\n';
    }

    bool updatePieceCoords(std::list<coords> &l, move &m) {
        for (coords &i : l) {
            if ((i.file == m.file1) && (i.rank == m.rank1)) {
                // update coords of the piece after moving it
                i.file = m.file2;
                i.rank = m.rank2;
                return true; // piece found, so return true
            }
        }
        return false;
    }


    void printPieceCoordsV(const std::vector<coords> &v) {
        std::cout << "\nPrint of all moves (vector): ";
        for (const coords &i : v) std::cout << i.file << i.rank << '\n';
        std::cout << '\n';
    }

    coords changePiece(std::vector<coords> &v, char file, int rank) {
        for (int i = 0; i < v.size(); ++i) {
            if ((v[i].file == file) && (v[i].rank == rank)) {
                
            }
        }
    }

    void updateCoordsV(gameStatus &gs, char file1, int rank1, char file2, int rank2) {
        for (int i = 0; i < gs.wherePieceAt.size(); ++i) {
            if ((gs.wherePieceAt[i].file == file1) && (gs.wherePieceAt[i].rank == rank1)) {
                gs.wherePieceAt[i].file = file2;
                gs.wherePieceAt[i].rank = rank2;
                return;
            }
        }
    }

    void deletePieceV(std::vector<coords> &v, char file, int rank) {
        for (int i = 0; i < v.size(); ++i)
            if ((v[i].file == file) && (v[i].rank == rank)) {
                v.erase(v.begin() + i);
                return;
            }
    }

}