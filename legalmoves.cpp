#include "chessengine.h"

namespace chess {

    bool promotePawn(board cb[ML][ML], unsigned char &player, int &rank1, int &file1) {
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

    bool rookMove(gameStatus &gs, int &rank1, int &file1, int &rank2, int &file2, bool isQueen) {
        if (!isQueen) { // since this function is also used to check queen moves, we have to know if we're passing a rook so if it moves it won't be able to be used to castle
            if ((rank1 == 1) && (file1 == 1)) gs.hasAlreadyMoved[1] = true;
            if ((rank1 == 1) && (file1 == 8)) gs.hasAlreadyMoved[2] = true;
            if ((rank1 == 8) && (file1 == 1)) gs.hasAlreadyMoved[4] = true;
            if ((rank1 == 8) && (file1 == 8)) gs.hasAlreadyMoved[5] = true;
        }

        if ((rank2 == rank1) != (file2 == file1)) { // rooks can move like a "+"; "!=" = "XOR" (both equations can't be true at the same time)
            if (rank1 < rank2) for (int trank = rank1 + 1; trank < rank2; ++trank) if (gs.chessboard[trank - 1][file2 - 1].piece) return false; // "cb[trank - 1][file2 - 1].piece" = "cb[trank - 1][file2 - 1].piece != EMPTY"
            if (rank1 > rank2) for (int trank = rank1 - 1; trank > rank2; --trank) if (gs.chessboard[trank - 1][file2 - 1].piece) return false;
            if (file1 < file2) for (int tfile = file1 + 1; tfile < file2; ++tfile) if (gs.chessboard[rank2 - 1][tfile - 1].piece) return false;
            if (file1 > file2) for (int tfile = file1 - 1; tfile > file2; --tfile) if (gs.chessboard[rank2 - 1][tfile - 1].piece) return false;
            return true; // if we arrived here it means there's no piece blocking the way
        } return false; // if we arrived here it means both the equations are true or both are false
    }

    bool bishopMove(board cb[ML][ML], int &rank1, int &file1, int &rank2, int &file2) {
        int trank, tfile;
        bool flag = false;
        if ((rank2 == rank1) || (file2 == file1)) return false; // if we arrive here it means the coords given don't correspond to how the bishop moves
        if ((rank2 > rank1) && (file2 > file1)) {
            for (trank = rank1, tfile = file1; trank < 9 || tfile < 9; ++trank, ++tfile) if ((rank2 == trank + 1) && (file2 == tfile + 1)) flag = true;
            if (!flag) return false;
            for (trank = rank1 + 1, tfile = file1 + 1; trank < rank2 || tfile < file2; ++trank, ++tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        } if ((rank2 > rank1) && (file2 < file1)) {
            for (trank = rank1, tfile = file1; trank < 9 || tfile > 0; ++trank, --tfile) if ((rank2 == trank + 1) && (file2 == tfile - 1)) flag = true;
            if (!flag) return false;
            for (trank = rank1 + 1, tfile = file1 - 1; trank < rank2 || tfile > file2; ++trank, --tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        } if ((rank2 < rank1) && (file2 > file1)) {
            for (trank = rank1, tfile = file1; trank > 0 || tfile < 9; --trank, ++tfile) if ((rank2 == trank - 1) && (file2 == tfile + 1)) flag = true;
            if (!flag) return false;
            for (trank = rank1 - 1, tfile = file1 + 1; trank > rank2 || tfile < file2; --trank, ++tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        } if ((rank2 < rank1) && (file2 < file1)) {
            for (trank = rank1, tfile = file1; trank > 0 || tfile > 0; --trank, --tfile) if ((rank2 == trank - 1) && (file2 == tfile - 1)) flag = true; 
            if (!flag) return false;
            for (trank = rank1 - 1, tfile = file1 - 1; trank > rank2 || tfile > file2; --trank, --tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        } return true;
    }

    // check if move is legit
    bool isMoveValid(gameStatus &gs, int rank1, int file1, int rank2, int file2) {

        // you can't take your own pieces
        if ((((gs.chessboard[rank2 - 1][file2 - 1].piece & PLAYERMASK) == WHITE) && (gs.player == WHITE)) || 
            (((gs.chessboard[rank2 - 1][file2 - 1].piece & PLAYERMASK) == BLACK) && (gs.player == BLACK))) return false;

        switch (gs.chessboard[rank1 - 1][file1 - 1].piece & PIECEMASK) { //! check which piece i'm using and get the legal moves
            
            case (PAWN):
                //TODO en passant
                if (file1 != file2) { // check if file2 is different to file1 (pawns can't move diagonally but they can take pieces on the square next to the diagonal)
                    if (!((file2 == file1 + 1) || (file2 == file1 - 1)) || gs.chessboard[rank2 - 1][file2 - 1].piece == EMPTY) return false;
                } else if (gs.chessboard[rank2 - 1][file2 - 1].piece != EMPTY) return false; // can't move in a square already occupied if the pawn isn't taking any pieces

                switch (gs.player) {
                    case (WHITE):
                        if (rank1 == 2) return (rank2 == 3 || rank2 == 4);     // if pawn is on starting position then it can move only 1 or 2 squares forward
                        if (rank2 != rank1 + 1) return false;                  // else it can only move 1 square forward
                        if (rank2 != 8) return true;                           // if rank2 != 8 then return true, otherwise promote the pawn
                        break;                                                 // break so it jumps to "return promotePawn(...)"
                    case (BLACK):                                              // same goes for BLACK pawns:
                        if (rank1 == 7) return (rank2 == 6 || rank2 == 5);     // starting square (1 or 2 squares)
                        if (rank2 != rank1 - 1) return false;                  // not starting square (1 square)
                        if (rank2 != 1) return true;                           // if not on promotion square simply return true
                        break;                                                 // otherwise brak to promote
                } return promotePawn(gs.chessboard, gs.player, rank1, file1);                // if we arrive here it means the only option left was the promotion
            case (KNIGHT): return (((rank2 == rank1 + 2) && (file2 == file1 - 1 || file2 == file1 + 1)) || ((rank2 == rank1 + 1) && (file2 == file1 - 2 || file2 == file1 + 2)) || ((rank2 == rank1 - 2) && (file2 == file1 - 1 || file2 == file1 + 1)) || ((rank2 == rank1 - 1) && (file2 == file1 - 2 || file2 == file1 + 2)));
            case (BISHOP): return bishopMove(gs.chessboard, rank1, file1, rank2, file2);
            case (ROOK): return rookMove(gs, rank1, file1, rank2, file2, false);
            case (QUEEN): return (bishopMove(gs.chessboard, rank1, file1, rank2, file2) || rookMove(gs, rank1, file1, rank2, file2, true)) ? true : false;
            case (KING): //TODO implement check/checkmate/castle
                // castle short & long
                if ((!gs.hasAlreadyMoved[0]) && (!gs.hasAlreadyMoved[2]) && (rank2 == 1) && (file2 == 7) && (gs.chessboard[rank2 - 1][file2 - 1].piece == EMPTY) && (gs.chessboard[0][5].piece == EMPTY)) {
                    gs.chessboard[0][5].piece = gs.chessboard[0][7].piece;  // move the rook
                    gs.chessboard[0][7].piece = EMPTY;                      // delete the rook before in previous position
                    gs.hasAlreadyMoved[0] = true;                           // set the king as "already moved" so it can't castle twice
                    return true;                                            
                } if ((!gs.hasAlreadyMoved[0]) && (!gs.hasAlreadyMoved[1]) && (rank2 == 1) && (file2 == 3) && (gs.chessboard[rank2 - 1][file2 - 1].piece == EMPTY) && (gs.chessboard[0][3].piece == EMPTY)) {
                    gs.chessboard[0][3].piece = gs.chessboard[0][0].piece;
                    gs.chessboard[0][0].piece = EMPTY;
                    gs.hasAlreadyMoved[0] = true;
                    return true;                                            
                } if ((!gs.hasAlreadyMoved[3]) && (!gs.hasAlreadyMoved[5]) && (rank2 == 8) && (file2 == 7) && (gs.chessboard[rank2 - 1][file2 - 1].piece == EMPTY) && (gs.chessboard[7][5].piece == EMPTY)) {
                    gs.chessboard[7][5].piece = gs.chessboard[7][7].piece;
                    gs.chessboard[7][7].piece = EMPTY;
                    gs.hasAlreadyMoved[3] = true;
                    return true;                                            
                } if ((!gs.hasAlreadyMoved[3]) && (!gs.hasAlreadyMoved[4]) && (rank2 == 8) && (file2 == 3) && (gs.chessboard[rank2 - 1][file2 - 1].piece == EMPTY) && (gs.chessboard[7][3].piece == EMPTY)) {
                    gs.chessboard[7][3].piece = gs.chessboard[7][0].piece;
                    gs.chessboard[7][0].piece = EMPTY;            
                    gs.hasAlreadyMoved[3] = true;
                    return true;                                            
                }
                // normal moves
                if ((rank2 == rank1 + 1) || (rank2 == rank1 - 1) || (rank2 == rank1))
                    if ((file2 == file1 + 1) || (file2 == file1 - 1) || (file2 == file1)) {
                        if (gs.player == WHITE) gs.hasAlreadyMoved[0] = true;
                        else gs.hasAlreadyMoved[3] = true;
                        return true;
                    };
                return false;  
            default: throw std::invalid_argument("Invalid piece");
        }
    }
    

}