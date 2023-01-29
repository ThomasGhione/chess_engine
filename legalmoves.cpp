#include "chessengine.h"

namespace chess {

    bool promotePawn(board cb[ML][ML], const unsigned char &player, const int &rank1, const int &file1) noexcept {
        char promotedPawn;
        std::cout << "What do you want to promote your pawn to?\nChoose between 'N', 'B', 'R' or 'Q': ";
        wrongOption:
        std::cin >> promotedPawn;
        
        switch (std::toupper(promotedPawn)) {
            case 'N': return (cb[rank1 - 1][file1 - 1].piece = player | KNIGHT);
            case 'B': return (cb[rank1 - 1][file1 - 1].piece = player | BISHOP);
            case 'R': return (cb[rank1 - 1][file1 - 1].piece = player | ROOK);
            case 'Q': return (cb[rank1 - 1][file1 - 1].piece = player | QUEEN);
            default:
                std::cout << "Option isn't valid! Choose again: ";
                goto wrongOption;
        }
    }

    bool rookMove(gameStatus &gs, const int &rank1, const int &file1, const int &rank2, const int &file2, bool isQueen) noexcept {
        if (!isQueen) { // since this function is also used to check queen moves, we have to know if we're passing a rook so if it moves it won't be able to be used to castle
            if ((rank1 == 1) && (file1 == 1)) gs.hasAlreadyMoved[1] = true;
            if ((rank1 == 1) && (file1 == 8)) gs.hasAlreadyMoved[2] = true;
            if ((rank1 == 8) && (file1 == 1)) gs.hasAlreadyMoved[4] = true;
            if ((rank1 == 8) && (file1 == 8)) gs.hasAlreadyMoved[5] = true;
        }

        // both equations can't be true at the same time, otherwise the rook would be able to move to the starting square (therefore skipping turn without moving at all)
        if (!((rank2 == rank1) != (file2 == file1))) return false;
        // if we arrive here it means the coords are legal, now we can check if there is any piece in the way 
        if (rank1 < rank2) for (int trank = rank1 + 1; trank < rank2; ++trank) if (gs.chessboard[trank - 1][file2 - 1].piece) return false; // the last if condition is an abbrevation for: "cb[trank - 1][file2 - 1].piece" = "cb[trank - 1][file2 - 1].piece != EMPTY"
        if (rank1 > rank2) for (int trank = rank1 - 1; trank > rank2; --trank) if (gs.chessboard[trank - 1][file2 - 1].piece) return false;
        if (file1 < file2) for (int tfile = file1 + 1; tfile < file2; ++tfile) if (gs.chessboard[rank2 - 1][tfile - 1].piece) return false;
        if (file1 > file2) for (int tfile = file1 - 1; tfile > file2; --tfile) if (gs.chessboard[rank2 - 1][tfile - 1].piece) return false;
        return true; // if we arrive here it means there's no piece blocking the way, therefore return true
    }

    bool bishopMove(board cb[ML][ML], const int &rank1, const int &file1, const int &rank2, const int &file2) noexcept {
        if ((rank2 == rank1) || (file2 == file1)) return false; // if rank2 == rank1 (same for file) then the move must be false
        if (!(abs((rank2 - rank1)) == abs((file2 - file1)))) return false;

        int trank, tfile;   // declaring the for counters once here so I don't have to declare it 4 times
        if ((rank2 > rank1) && (file2 > file1)) for (trank = rank1 + 1, tfile = file1 + 1; trank < rank2 || tfile < file2; ++trank, ++tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        if ((rank2 > rank1) && (file2 < file1)) for (trank = rank1 + 1, tfile = file1 - 1; trank < rank2 || tfile > file2; ++trank, --tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        if ((rank2 < rank1) && (file2 > file1)) for (trank = rank1 - 1, tfile = file1 + 1; trank > rank2 || tfile < file2; --trank, ++tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        if ((rank2 < rank1) && (file2 < file1)) for (trank = rank1 - 1, tfile = file1 - 1; trank > rank2 || tfile > file2; --trank, --tfile) if (cb[trank - 1][tfile - 1].piece) return false;
        return true;
    }

    // check if move is legit
    bool isMoveValid(gameStatus &gs, int rank1, int file1, int rank2, int file2) noexcept {
/*
        if (gs.check.flag) {
            if ((gs.player == WHITE) && (gs.chessboard[rank1 -1 ][file1 - 1].piece != (WHITE | KING))) return false;
            if ((gs.player == BLACK) && (gs.chessboard[rank1 -1 ][file1 - 1].piece != (BLACK | KING))) return false;
        }
*/

        switch (gs.chessboard[rank1 - 1][file1 - 1].piece & PIECEMASK) { //! check which piece i'm using and get the legal moves
            
            case (PAWN):
                if (file1 != file2) { // check if file2 is different to file1 (pawns can't move diagonally but they can take pieces on the square next to the diagonal)
                    if ((gs.player == WHITE) && (gs.lastMove.file1 == gs.lastMove.file2) && (gs.lastMove.rank1 == 7) && (gs.lastMove.rank2 == 5)) {
                        if ((rank2 == rank1 + 1) && (file2 == fromCharToInt(gs.lastMove.file2)) && ((file2 == file1 + 1) || (file2 == file1 - 1))) {
                            gs.chessboard[gs.lastMove.rank2 - 1][fromCharToInt(gs.lastMove.file2) - 1].piece = EMPTY; // piece taken with en passant
                            return true;
                        } 
                    } if ((gs.player == BLACK) && (gs.lastMove.file1 == gs.lastMove.file2) && (gs.lastMove.rank1 == 2) && (gs.lastMove.rank2 == 4)) {
                        if ((rank2 == rank1 - 1) && (file2 == fromCharToInt(gs.lastMove.file2)) && ((file2 == file1 + 1) || (file2 == file1 - 1))) {
                            gs.chessboard[gs.lastMove.rank2 - 1][fromCharToInt(gs.lastMove.file2) - 1].piece = EMPTY; // piece taken with en passant
                            return true;
                        } 
                    }
                    // normal way for a pawn to eat a piece
                    if (!((file2 == file1 + 1) || (file2 == file1 - 1)) || gs.chessboard[rank2 - 1][file2 - 1].piece == EMPTY) return false;
                } else if (gs.chessboard[rank2 - 1][file2 - 1].piece != EMPTY) return false; // can't move to an already occupied square if the pawn isn't taking a piece

                switch (gs.player) {
                    case (WHITE): 
                        // TODO: fix potential bug in rank2 == 3
                        if (rank1 == 2) return (rank2 == 3 || ((rank2 == 4) && (!gs.chessboard[2][file2 - 1].piece))); // if pawn is on starting position then it can move only 1 or 2 squares forward
                        if (rank2 != rank1 + 1) return false;                 // else it can only move 1 square forward                        
                        if (rank2 != 8) return true;                          // if rank2 != 8 then return true, otherwise promote the pawn
                        break;                                                // break so it jumps to "return promotePawn(...)"
                    case (BLACK):
                        if (rank1 == 7) return (rank2 == 6 || ((rank2 == 5) && (!gs.chessboard[5][file2 - 1].piece)));
                        if (rank2 != rank1 - 1) return false;
                        if (rank2 != 1) return true;
                        break;
                } return promotePawn(gs.chessboard, gs.player, rank1, file1); // if we arrive here it means the only option left was the promotion
            case (KNIGHT): return (((rank2 == rank1 + 2) && (file2 == file1 - 1 || file2 == file1 + 1)) || ((rank2 == rank1 + 1) && (file2 == file1 - 2 || file2 == file1 + 2)) || ((rank2 == rank1 - 2) && (file2 == file1 - 1 || file2 == file1 + 1)) || ((rank2 == rank1 - 1) && (file2 == file1 - 2 || file2 == file1 + 2)));
            case (BISHOP): return bishopMove(gs.chessboard, rank1, file1, rank2, file2);
            case (ROOK): return rookMove(gs, rank1, file1, rank2, file2, false);
            case (QUEEN): return (bishopMove(gs.chessboard, rank1, file1, rank2, file2) || rookMove(gs, rank1, file1, rank2, file2, true)) ? true : false;
            case (KING): //TODO: implement check/checkmate

                // check for non-castle moves, if it moved normally then we set the hasAlreadyMoved to true
                if (((rank2 == rank1 + 1) || (rank2 == rank1 - 1) || (rank2 == rank1)) && ((file2 == file1 + 1) || (file2 == file1 - 1) || (file2 == file1))) {
                    (gs.player == WHITE) ? (gs.hasAlreadyMoved[0] = true) : (gs.hasAlreadyMoved[3] = true);
                    return true;
                }

                // TODO: we can optimize this :)
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
                
                return false; // if we arrive here it means we already checked for every possibility therefore the move isn't valid
        }

        // if we arrive here it means something went wrong with recognizing the piece id
        std::cout << "The piece you tried to move seemed to have a problem the program couldn't understand, try again or reboot the program. :(\n";
        return false; 
    }
    





    // list of moves
    // TODO: NOT FINISHED
    //! PROTOTYPE OF NEW FUNCTION TO CALCULATE EVERY MOVE (WILL BE THE NEW "IsMoveValid")
    void everyLegalMove(gameStatus &gs) { //TODO "returns are placeholders"
        
        for (int rank = 0; rank < ML; ++rank) {
            for (int file = 0; file < ML; ++file) {
                if ((gs.chessboard[file][rank].piece != EMPTY) && (gs.player == (gs.chessboard[file][rank].piece & PLAYERMASK))) {
                    
                    //! PAWN
                    if ((gs.chessboard[file][rank].piece & PIECEMASK) == PAWN) {
                        if (gs.chessboard[file][rank].piece == (WHITE | PAWN)) {

                        }
                        if (gs.chessboard[file][rank].piece == (BLACK | PAWN)) {
                            
                        }

                    }

                    //! ROOK
                    if ((gs.chessboard[file][rank].piece & PIECEMASK) == ROOK) {
                        for (int it_file = file - 1; it_file >= 0; --it_file) {
                            if (gs.chessboard[it_file][rank].piece == EMPTY) { // if square is empty then move is legal
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(it_file + 'a'), rank); // update the square of the piece
                                return;
                            }
                            if ((gs.chessboard[it_file][rank].piece & PLAYERMASK) != gs.player) {
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(it_file + 'a'), rank); // update the square of the piece
                                deletePieceV(gs.wherePieceAt, (char)(it_file + 'a'), rank);                  // delete the taken piece
                                return; // if there is an opponent piece move is legal
                            }
                            return; // if we arrive here it means we hit your own pieces 
                        }
                        for (int it_file = file + 1; it_file < ML; ++it_file) {
                            if (gs.chessboard[it_file][rank].piece == EMPTY) {
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(it_file + 'a'), rank);
                                return;
                            }
                            if ((gs.chessboard[it_file][rank].piece & PLAYERMASK) != gs.player) {
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(it_file + 'a'), rank);
                                deletePieceV(gs.wherePieceAt, (char)(it_file + 'a'), rank);
                                return; // if there is an opponent piece move is legal
                            }
                            return; 
                        }
                        for (int it_rank = rank - 1; it_rank >= 0; --it_rank) {
                            if (gs.chessboard[file][it_rank].piece == EMPTY) {
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(file + 'a'), it_rank);
                                return;
                            }
                            if ((gs.chessboard[file][it_rank].piece & PLAYERMASK) != gs.player) {
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(file + 'a'), it_rank);
                                deletePieceV(gs.wherePieceAt, (char)(file + 'a'), it_rank);
                                return; // if there is an opponent piece move is legal
                            }
                            return;
                        }
                        for (int it_rank = rank + 1; it_rank < ML; ++it_rank) {
                            if (gs.chessboard[file][it_rank].piece == EMPTY) {
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(file + 'a'), it_rank);
                                return;
                            }
                            if ((gs.chessboard[file][it_rank].piece & PLAYERMASK) != gs.player) {
                                updateCoordsV(gs, (char)(file + 'a'), rank, (char)(file + 'a'), it_rank);
                                deletePieceV(gs.wherePieceAt, (char)(file + 'a'), it_rank);
                                return; // if there is an opponent piece move is legal
                            }
                            return;
                        }
                    }

                    //! BISHOP
                    if ((gs.chessboard[file][rank].piece & PIECEMASK) == BISHOP) {
                        int it_file, it_rank;
                        for (it_file = file - 1, it_rank = rank - 1; it_file >= 0 && it_rank >= 0; --it_file, --it_rank) {
                            if (gs.chessboard[it_file][it_rank].piece == EMPTY) {
                                return;
                            }
                            if ((gs.chessboard[it_file][it_rank].piece & PLAYERMASK) != gs.player) {
                                return;
                            }
                            return;                            
                        }
                        for (it_file = file + 1, it_rank = rank - 1; it_file < ML && it_rank >= 0; ++it_file, --it_rank) {
                            if (gs.chessboard[it_file][it_rank].piece == EMPTY) {
                                return;
                            }
                            if ((gs.chessboard[it_file][it_rank].piece & PLAYERMASK) != gs.player) {
                                return;
                            }
                            return;                            
                        }
                        for (it_file = file - 1, it_rank = rank + 1; it_file >= 0 && it_rank < ML; --it_file, ++it_rank) {
                            if (gs.chessboard[it_file][it_rank].piece == EMPTY) {
                                return;
                            }
                            if ((gs.chessboard[it_file][it_rank].piece & PLAYERMASK) != gs.player) {
                                return;
                            }
                            return;                            
                        }
                        for (it_file = file + 1, it_rank = rank + 1; it_file < ML && it_rank < ML; ++it_file, ++it_rank) {
                            if (gs.chessboard[it_file][it_rank].piece == EMPTY) {
                                return;
                            }
                            if ((gs.chessboard[it_file][it_rank].piece & PLAYERMASK) != gs.player) {
                                return;
                            }
                            return;                            
                        }
                    }
                    
                }
            } 
        }

        return;
    }
    
    // if piece == black => blacklegalmoves


}