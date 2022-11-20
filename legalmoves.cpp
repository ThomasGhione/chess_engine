#include "chessengine.h"

namespace chess {

    // check if move is legit
    bool IsMoveValid(char player, unsigned char piece, board chessboard[ML][ML], int rank1, int file1, int rank2, int file2) {

        // check if piece is white or black
        if (player == 'W') piece -= WHITE;  // piece is white
        else piece -= BLACK;                // piece is black


        // check which piece i'm using and get the legal moves
        switch (piece) {

            case (PAWN):
                // if pawn is white it goes '^' otherwise it goes 'v'
                if (player == 'W') {
                    
                } else {

                }

            case (KNIGHT):

            case (BISHOP):

            case (ROOK):
                if (rank2 == rank1) return true;
                if (file2 == file1) return true;
                return false;
                
                
            case (QUEEN):

            case (KING):


            default: throw std::invalid_argument("Invalid piece");
        }
    }


}