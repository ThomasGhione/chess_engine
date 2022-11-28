#include "chessengine.h"

namespace chess {

    // check if move is legit
    bool isMoveValid(char player, board chessboard[ML][ML], int rank1, int file1, int rank2, int file2) {
        std::cout << (chessboard[rank1][file1].piece == (WHITE | PAWN)) << std::endl; // 01 000001
        // get the piece ID by subtracting the 1st 2bits
        //(player == 'W') ? chessboard[rank1][file1].piece -= WHITE : chessboard[rank1][file1].piece -= BLACK;      
        chessboard[rank1][file1].piece &= PIECEMASK; 
        std::cout << (chessboard[rank1][file1].piece == PAWN) << std::endl;
        // check which piece i'm using and get the legal moves
        switch (chessboard[rank1][file1].piece) {

            case (PAWN):
                if (file1 != file2) return false; // pawn can't move diagonally, if so then return false

                // if pawn is white it goes '^' otherwise it goes 'v'
                if (player == 'W') {
                    if (rank1 == 2) // if pawn is on starting position then it can move either 1 or 2 squares forward
                        return (rank2 == 3 || rank2 == 4); 
                    if (rank1 > 2) { // else it can move only 1 square
                        if (rank2 == 8) return true; //TODO promote pawn 
                        return (rank2 == rank1 + 1);
                    } return false; // TODO throw exception
                }
                if (player == 'B') {
                    if (rank1 == 7)
                        return (rank2 == 6 || rank2 == 5);
                    if (rank1 < 7) {
                        if (rank2 == 1) return true; //TODO promote pawn
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