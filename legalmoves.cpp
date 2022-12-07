#include "chessengine.h"

namespace chess {

    bool promotePawn(unsigned char player, board chessboard[ML][ML], int rank1, int file1) {
        char promotedPawn;
        std::cout << "What do you want to promote your pown to?\nChoose between 'N', 'B', 'R' or 'Q': ";
        wrongOption:
        std::cin >> promotedPawn;
        if (promotedPawn != 'N' && promotedPawn != 'B' && promotedPawn != 'R' && promotedPawn != 'Q') {
            std::cout << "Option isn't valid! Choose again: ";
            goto wrongOption;
        }
        
        switch (promotedPawn) {
            case 'N': chessboard[rank1 - 1][file1 - 1].piece = player | KNIGHT; return true;
            case 'B': chessboard[rank1 - 1][file1 - 1].piece = player | BISHOP; return true;
            case 'R': chessboard[rank1 - 1][file1 - 1].piece = player | ROOK; return true;
            case 'Q': chessboard[rank1 - 1][file1 - 1].piece = player | QUEEN; return true;
            default: throw std::invalid_argument("invalid argument");
        }
    }

    // check if move is legit
    bool isMoveValid(unsigned char player, board chessboard[ML][ML], int rank1, int file1, int rank2, int file2) {

        // you can't take your own pieces
        if (((chessboard[rank2 - 1][file2 - 1].piece & PLAYERMASK) == WHITE) && (player == 'W')) return false;
        if (((chessboard[rank2 - 1][file2 - 1].piece & PLAYERMASK) == BLACK) && (player == 'B')) return false;

        switch (chessboard[rank1 - 1][file1 - 1].piece & PIECEMASK) { //! check which piece i'm using and get the legal moves
            
            case (PAWN):
                //TODO en passant
                if (file1 != file2) // check if file2 is different to file1 (pawns can't move diagonally but they can take pieces on the square next to the diagonal)
                    if (!((file1 == file2 + 1) || (file1 == file2 - 1)) || chessboard[rank2 - 1][file2 - 1].piece == EMPTY) return false; 

                if (player == WHITE) {

                    if (rank1 == 2) // if pawn is on starting position then it can move either 1 or 2 squares forward
                        return (rank2 == 3 || rank2 == 4); 
                    if (rank1 > 2) { // else it can move only 1 square
                        if (rank2 != rank1 + 1) return false; // can't move more than 1 square at time  
                        if (rank2 != 8) return true; // if rank2 != 8 then return true, otherwise promote the pawn
                        return promotePawn(player, chessboard, rank1, file1);
                    } throw std::invalid_argument("invalid pawn");
                }
                if (player == BLACK) {
                    if (rank1 == 7)
                        return (rank2 == 6 || rank2 == 5);
                    if (rank1 < 7) {
                        if (rank2 != rank1 - 1) return false;
                        if (rank2 != 1) return true;
                        return promotePawn(player, chessboard, rank1, file1);
                    } throw std::invalid_argument("invalid pawn");
                }
                throw std::invalid_argument("pawn is neither white or black");
            case (KNIGHT):
                return (((rank2 == rank1 + 2) && (file2 == file1 - 1 || file2 == file1 + 1)) ||
                        ((rank2 == rank1 + 1) && (file2 == file1 - 2 || file2 == file1 + 2)) ||
                        ((rank2 == rank1 - 2) && (file2 == file1 - 1 || file2 == file1 + 1)) ||
                        ((rank2 == rank1 - 1) && (file2 == file1 - 2 || file2 == file1 + 2)));
            case (BISHOP):
                int trank, tfile;
                for (trank = rank1, tfile = file1; trank < 9 || tfile < 9; ++trank, ++tfile) 
                    if ((rank2 == trank + 1) && (file2 == tfile + 1)) return true;
                for (trank = rank1, tfile = file1; trank < 9 || tfile; ++trank, --tfile) 
                    if ((rank2 == trank + 1) && (file2 == tfile - 1)) return true;
                for (trank = rank1, tfile = file1; trank || tfile < 9; --trank, ++tfile) 
                    if ((rank2 == trank - 1) && (file2 == tfile + 1)) return true;
                for (trank = rank1, tfile = file1; trank || tfile; --trank, --tfile) 
                    if ((rank2 == trank - 1) && (file2 == tfile - 1)) return true;
                return false;
            case (ROOK):
                return ((rank2 == rank1) != (file2 == file1)); // if rank (or file) is the same then move is valid (!= means XOR)
            case (QUEEN):
                if ((rank2 == rank1) != (file2 == file1)) return true;
                for (trank = rank1, tfile = file1; trank < 9 || tfile < 9; ++trank, ++tfile) 
                    if ((rank2 == trank + 1) && (file2 == tfile + 1)) return true;
                for (trank = rank1, tfile = file1; trank < 9 || tfile; ++trank, --tfile) 
                    if ((rank2 == trank + 1) && (file2 == tfile - 1)) return true;
                for (trank = rank1, tfile = file1; trank || tfile < 9; --trank, ++tfile) 
                    if ((rank2 == trank - 1) && (file2 == tfile + 1)) return true;
                for (trank = rank1, tfile = file1; trank || tfile; --trank, --tfile) 
                    if ((rank2 == trank - 1) && (file2 == tfile - 1)) return true;
                return false;
            case (KING): //TODO implement check/checkmate/castle
                if ((rank2 == rank1 + 1) || (rank2 == rank1 - 1) || (rank2 == rank1))
                    return ((file2 == file1 + 1) || (file2 == file1 - 1) || (file2 == file1));
                return false;  
            default: throw std::invalid_argument("Invalid piece");
        }
    }
    

}