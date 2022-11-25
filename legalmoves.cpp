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
                    if (file1 != file2) return false; // pawn can't move diagonally, if so then return false

                    if (rank1 == 1) // if pawn is on starting position then it can move either 1 or 2 squares forward
                        return (rank2 == 2 || rank2 == 3); 
                    if (rank1 > 1) // else it can move only 1 square
                        if (rank2 == 7) return true; //TODO promote pawn 
                        return (rank2 == rank1 + 1);
                    return false; // TODO throw exception
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
                return (rank2 == rank1 || file2 == file1); // if rank (or file) is the same then move is valid 


            case (QUEEN):
                return true; //! PLACEHOLDER
            case (KING):
                return true; //! PLACEHOLDER
            default: throw std::invalid_argument("Invalid piece");
        }
    }
    

}