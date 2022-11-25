#include "chessengine.h"

namespace chess {

    // check if move is legit
    bool isMoveValid(char player, board chessboard[ML][ML], int rank1, int file1, int rank2, int file2) {

        // get the piece ID by subtracting the 1st 2bits
        (player == 'W') ? chessboard[rank1][file1].piece -= WHITE : chessboard[rank1][file1].piece -= BLACK;      

        // check which piece i'm using and get the legal moves
        switch (chessboard[rank1][file1].piece) {

            case (PAWN):
                // if pawn is white it goes '^' otherwise it goes 'v'
                if (player == 'W') {
                    if (rank2 > rank1) return true;
                    if (file2 != file1) return false;
                }
                else {
                    if (rank2 < rank1) return true;
                    if (file2 != file1) return false;
                }
                return true; //! PLACEHOLDER
            case (KNIGHT):
                return true; //! PLACEHOLDER
            case (BISHOP):
                return true; //! PLACEHOLDER
            case (ROOK):
                if (rank2 == rank1) return true;
                if (file2 == file1) return true;
                return false;
            case (QUEEN):
                return true; //! PLACEHOLDER
            case (KING):
                return true; //! PLACEHOLDER
            default: throw std::invalid_argument("Invalid piece");
        }
    }
    

}