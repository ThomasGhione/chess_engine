#include "chessengine.h"

namespace chess {

    bool promotePawn(unsigned char player, board cb[ML][ML], int rank1, int file1) {
        char promotedPawn;
        std::cout << "What do you want to promote your pown to?\nChoose between 'N', 'B', 'R' or 'Q': ";
        wrongOption:
        std::cin >> promotedPawn;
        if (promotedPawn != 'N' && promotedPawn != 'B' && promotedPawn != 'R' && promotedPawn != 'Q') {
            std::cout << "Option isn't valid! Choose again: ";
            goto wrongOption;
        }
        
        switch (promotedPawn) {
            case 'N': cb[rank1 - 1][file1 - 1].piece = player | KNIGHT; return true;
            case 'B': cb[rank1 - 1][file1 - 1].piece = player | BISHOP; return true;
            case 'R': cb[rank1 - 1][file1 - 1].piece = player | ROOK; return true;
            case 'Q': cb[rank1 - 1][file1 - 1].piece = player | QUEEN; return true;
            default: throw std::invalid_argument("invalid argument");
        }
    }

    // check if move is legit
    bool isMoveValid(unsigned char player, board cb[ML][ML], int rank1, int file1, int rank2, int file2) {

        // you can't take your own pieces
        if (((cb[rank2 - 1][file2 - 1].piece & PLAYERMASK) == WHITE) && (player == WHITE)) return false;
        if (((cb[rank2 - 1][file2 - 1].piece & PLAYERMASK) == BLACK) && (player == BLACK)) return false;

        switch (cb[rank1 - 1][file1 - 1].piece & PIECEMASK) { //! check which piece i'm using and get the legal moves
            
            case (PAWN):
                //TODO en passant
                if (file1 != file2) // check if file2 is different to file1 (pawns can't move diagonally but they can take pieces on the square next to the diagonal)
                    if (!((file1 == file2 + 1) || (file1 == file2 - 1)) || cb[rank2 - 1][file2 - 1].piece == EMPTY) return false; 

                if (cb[rank2 - 1][file2 - 1].piece != EMPTY) return false; // can't move in a square already occupied

                if (player == WHITE) {

                    if (rank1 == 2) // if pawn is on starting position then it can move either 1 or 2 squares forward
                        return (rank2 == 3 || rank2 == 4); 
                    if (rank1 > 2) { // else it can move only 1 square
                        if (rank2 != rank1 + 1) return false; // can't move more than 1 square at time  
                        if (rank2 != 8) return true; // if rank2 != 8 then return true, otherwise promote the pawn
                        return promotePawn(player, cb, rank1, file1);
                    } throw std::invalid_argument("invalid pawn");
                }
                if (player == BLACK) {
                    if (rank1 == 7)
                        return (rank2 == 6 || rank2 == 5);
                    if (rank1 < 7) {
                        if (rank2 != rank1 - 1) return false;
                        if (rank2 != 1) return true;
                        return promotePawn(player, cb, rank1, file1);
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
                if ((rank2 == rank1) != (file2 == file1)) { // rooks can move like a "+", "!="" = "XOR" (both equations can't be true at the same time)
                    if (rank1 < rank2) for (int trank = rank1 + 1; trank < rank2; ++trank) if (cb[trank - 1][file2 - 1].piece) return false; // "cb[trank - 1][file2 - 1].piece" = "cb[trank - 1][file2 - 1].piece != EMPTY"
                    if (rank1 > rank2) for (int trank = rank1 - 1; trank > rank2; --trank) if (cb[trank - 1][file2 - 1].piece) return false;
                    if (file1 < file2) for (int tfile = file1 + 1; tfile < file2; ++tfile) if (cb[rank2 - 1][tfile - 1].piece) return false;
                    if (file1 > file2) for (int tfile = file1 - 1; tfile > file2; --tfile) if (cb[rank2 - 1][tfile - 1].piece) return false;
                    return true; // if we arrived here it means there's no piece blocking the way
                } return false; // if we arrived here it means both the equations are true or both are false
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