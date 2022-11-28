#include "chessengine.h"

namespace chess {

    // check if move is legit
    bool isMoveValid(char player, board chessboard[ML][ML], int rank1, int file1, int rank2, int file2) {
        unsigned char piece1 = chessboard[rank1][file1].piece & PIECEMASK; // get the piece ID by getting the 1st 2bits = to zero   

        //! check which piece i'm using and get the legal moves
        switch (piece1) {
            
            case (PAWN):
                if (file1 != file2) return false; // pawn can't move diagonally, if so then return false

                // if pawn is white it goes '^' otherwise it goes 'v'
                if (player == 'W') {
                    if (rank1 == 1) // if pawn is on starting position then it can move either 1 or 2 squares forward
                        return (rank2 == 2 || rank2 == 3); 
                    if (rank1 > 1) { // else it can move only 1 square
                        if (rank2 == 7) return true; //TODO promote pawn 
                        return (rank2 == rank1 + 1);
                    } return false; // TODO throw exception
                }
                if (player == 'B') {
                    if (rank1 == 6)
                        return (rank2 == 5 || rank2 == 4);
                    if (rank1 < 6) {
                        if (rank2 == 0) return true; //TODO promote pawn
                        return (rank2 == rank1 - 1);
                    } return false; // TODO throw exception
                }
                throw std::invalid_argument("pawn isn't white nor black");
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
            //default: throw std::invalid_argument("Invalid piece");
            default: return true; //TODO REMOVE THIS PLACEHOLDER
        }
    }
    

}