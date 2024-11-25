#include "chessengine.h"

namespace chess {

    bool promotePawn(board cb[ML][ML], const unsigned char &player, const move& m) noexcept {
        char promotedPawn;
        cout << "What do you want to promote your pawn to?\nChoose between 'N', 'B', 'R' or 'Q': ";
        wrongOption:
        cin >> promotedPawn;
        
        switch (toupper(promotedPawn)) {
            case 'N':
                return (cb[m.rank - 1][ctoi(m.file) - 1].piece.id = player | KNIGHT);
            case 'B':
                return (cb[m.rank - 1][ctoi(m.file) - 1].piece.id = player | BISHOP);
            case 'R':
                return (cb[m.rank - 1][ctoi(m.file) - 1].piece.id = player | ROOK);
            case 'Q':
                return (cb[m.rank - 1][ctoi(m.file) - 1].piece.id = player | QUEEN);
            default:
                cout << "Option isn't valid! Choose again: ";
                goto wrongOption;
        }
    }

    bool rookMove(gameStatus &gs, const move& m) noexcept {
        // since this function is also used to check queen moves,
        // we have to know if we're passing a rook so if it moves it won't be able to be used to castle
        if ((m.id & PIECEMASK) == ROOK) { 
            if ((m.rank == 1) && (m.file == 1))
                gs.hasAlreadyMoved[1] = true;
            if ((m.rank == 1) && (m.file == 8))
                gs.hasAlreadyMoved[2] = true;
            if ((m.rank == 8) && (m.file == 1))
                gs.hasAlreadyMoved[4] = true;
            if ((m.rank == 8) && (m.file == 8))
                gs.hasAlreadyMoved[5] = true;
        }

        if (m.rank != m.movesTo.rank && m.file != m.movesTo.file)
            return false;

        const int dRank = (m.movesTo.rank > m.rank) ? 1 : (m.movesTo.rank < m.rank) ? -1 : 0;
        const int dFile = (m.movesTo.file > m.file) ? 1 : (m.movesTo.file < m.file) ? -1 : 0;

        for (int trank = m.rank + dRank, tfile = m.file + dFile; (trank != m.movesTo.rank) || (tfile != m.movesTo.file); trank += dRank, tfile += dFile)
            if (gs.chessboard[trank - 1][tfile - 1].piece.id)
                return false;

        return true;
    }

    bool bishopMove(board cb[ML][ML], const move& m) noexcept {
        if ((m.movesTo.rank == m.rank) || (m.movesTo.file == m.file))
            return false;

        const int dRank = (m.movesTo.rank > m.rank) ? 1 : -1;
        const int dFile = (m.movesTo.file > m.file) ? 1 : -1;

        int trank = m.rank + dRank, tfile = m.file + dFile;
        while (trank != m.movesTo.rank || tfile != m.movesTo.file) {
            if (cb[trank - 1][tfile - 1].piece.id)
                return false;
            trank += dRank;
            tfile += dFile;
        }

        return true;
    }

    // check if move is legit
    bool isMoveValid(gameStatus &gs, move m) noexcept {
/*
        if (gs.check.flag) {
            if ((gs.player == WHITE) && (gs.chessboard[m.rank -1 ][ctoi(m.file) - 1].piece.id != (WHITE | KING))) return false;
            if ((gs.player == BLACK) && (gs.chessboard[m.rank -1 ][ctoi(m.file) - 1].piece.id != (BLACK | KING))) return false;
        }
*/

        switch (gs.chessboard[m.rank - 1][ctoi(m.file) - 1].piece.id & PIECEMASK) { //! check which piece i'm using and get the legal moves
            
            case (PAWN):
                if (m.file != m.movesTo.file) { // check if m.movesTo.file is different to m.file (pawns can't move diagonally but they can take pieces on the square next to the diagonal)
                    if ((gs.player == WHITE) && (gs.lastMove.file == gs.lastMove.movesTo.file) && (gs.lastMove.rank == 7) && (gs.lastMove.movesTo.rank == 5)) {
                        if ((m.movesTo.rank == m.rank + 1) && (m.movesTo.file == ctoi(gs.lastMove.movesTo.file)) && ((m.movesTo.file == m.file + 1) || (m.movesTo.file == m.file - 1))) {
                            gs.chessboard[gs.lastMove.movesTo.rank - 1][ctoi(gs.lastMove.movesTo.file) - 1].piece.id = EMPTY; // piece taken with en passant
                            return true;
                        } 
                    }
                    if ((gs.player == BLACK) && (gs.lastMove.file == gs.lastMove.movesTo.file) && (gs.lastMove.rank == 2) && (gs.lastMove.movesTo.rank == 4)) {
                        if ((m.movesTo.rank == m.rank - 1) && (m.movesTo.file == ctoi(gs.lastMove.movesTo.file)) && ((m.movesTo.file == m.file + 1) || (m.movesTo.file == m.file - 1))) {
                            gs.chessboard[gs.lastMove.movesTo.rank - 1][ctoi(gs.lastMove.movesTo.file) - 1].piece.id = EMPTY; // piece taken with en passant
                            return true;
                        } 
                    }
                    // normal way for a pawn to eat a piece
                    if (((m.movesTo.file == m.file + 1) || (m.movesTo.file == m.file - 1)) && gs.chessboard[m.movesTo.rank - 1][m.movesTo.file - 1].piece.id != EMPTY)
                        return false;
                }
                else if (gs.chessboard[m.movesTo.rank - 1][m.movesTo.file - 1].piece.id != EMPTY)
                    return false; // can't move to an already occupied square if the pawn isn't taking a piece

                switch (gs.player) {
                    case (WHITE): 
                        // TODO: fix potential bug in m.movesTo.rank == 3
                        if (m.rank == 2)
                            return (m.movesTo.rank == 3 || ((m.movesTo.rank == 4) && (!gs.chessboard[2][m.movesTo.file - 1].piece.id))); // if pawn is on starting position then it can move only 1 or 2 squares forward
                        if (m.movesTo.rank != m.rank + 1)
                            return false;                 // else it can only move 1 square forward                        
                        if (m.movesTo.rank != 8)
                            return true;                          // if m.movesTo.rank != 8 then return true, otherwise promote the pawn
                        break;                                                // break so it jumps to "return promotePawn(...)"
                    case (BLACK):
                        if (m.rank == 7)
                            return (m.movesTo.rank == 6 || ((m.movesTo.rank == 5) && (!gs.chessboard[5][m.movesTo.file - 1].piece.id)));
                        if (m.movesTo.rank != m.rank - 1)
                            return false;
                        if (m.movesTo.rank != 1)
                            return true;
                        break;
                }
                return promotePawn(gs.chessboard, gs.player, m); // if we arrive here it means the only option left was the promotion
            case (KNIGHT):
                return (((m.movesTo.rank == m.rank + 2) && (m.movesTo.file == m.file - 1 || m.movesTo.file == m.file + 1)) || ((m.movesTo.rank == m.rank + 1) && (m.movesTo.file == m.file - 2 || m.movesTo.file == m.file + 2)) || ((m.movesTo.rank == m.rank - 2) && (m.movesTo.file == m.file - 1 || m.movesTo.file == m.file + 1)) || ((m.movesTo.rank == m.rank - 1) && (m.movesTo.file == m.file - 2 || m.movesTo.file == m.file + 2)));
            case (BISHOP):
                return bishopMove(gs.chessboard, m);
            case (ROOK):
                return rookMove(gs, m);
            case (QUEEN):
                return bishopMove(gs.chessboard, m) || rookMove(gs, m);
            case (KING): //TODO: implement check/checkmate

                // check for non-castle moves, if it moved normally then we set the hasAlreadyMoved to true
                if (((m.movesTo.rank == m.rank + 1) || (m.movesTo.rank == m.rank - 1) || (m.movesTo.rank == m.rank)) && ((m.movesTo.file == m.file + 1) || (m.movesTo.file == m.file - 1) || (m.movesTo.file == m.file))) {
                    (gs.player == WHITE)
                        ? (gs.hasAlreadyMoved[0] = true)
                        : (gs.hasAlreadyMoved[3] = true);
                    return true;
                }

                // TODO: we can optimize this :)
                // castle short & long
                if ((!gs.hasAlreadyMoved[0]) && (!gs.hasAlreadyMoved[2]) && (m.movesTo.rank == 1) && (m.movesTo.file == 7) && (gs.chessboard[m.movesTo.rank - 1][m.movesTo.file - 1].piece.id == EMPTY) && (gs.chessboard[0][5].piece.id == EMPTY)) {
                    gs.chessboard[0][5].piece.id = gs.chessboard[0][7].piece.id;  // move the rook
                    gs.chessboard[0][7].piece.id = EMPTY;                      // delete the rook before in previous position
                    gs.hasAlreadyMoved[0] = true;                           // set the king as "already moved" so it can't castle twice
                    return true;                                            
                }
                if ((!gs.hasAlreadyMoved[0]) && (!gs.hasAlreadyMoved[1]) && (m.movesTo.rank == 1) && (m.movesTo.file == 3) && (gs.chessboard[m.movesTo.rank - 1][m.movesTo.file - 1].piece.id == EMPTY) && (gs.chessboard[0][3].piece.id == EMPTY)) {
                    gs.chessboard[0][3].piece.id = gs.chessboard[0][0].piece.id;
                    gs.chessboard[0][0].piece.id = EMPTY;
                    gs.hasAlreadyMoved[0] = true;
                    return true;                                            
                }
                if ((!gs.hasAlreadyMoved[3]) && (!gs.hasAlreadyMoved[5]) && (m.movesTo.rank == 8) && (m.movesTo.file == 7) && (gs.chessboard[m.movesTo.rank - 1][m.movesTo.file - 1].piece.id == EMPTY) && (gs.chessboard[7][5].piece.id == EMPTY)) {
                    gs.chessboard[7][5].piece.id = gs.chessboard[7][7].piece.id;
                    gs.chessboard[7][7].piece.id = EMPTY;
                    gs.hasAlreadyMoved[3] = true;
                    return true;                                            
                }
                if ((!gs.hasAlreadyMoved[3]) && (!gs.hasAlreadyMoved[4]) && (m.movesTo.rank == 8) && (m.movesTo.file == 3) && (gs.chessboard[m.movesTo.rank - 1][m.movesTo.file - 1].piece.id == EMPTY) && (gs.chessboard[7][3].piece.id == EMPTY)) {
                    gs.chessboard[7][3].piece.id = gs.chessboard[7][0].piece.id;
                    gs.chessboard[7][0].piece.id = EMPTY;            
                    gs.hasAlreadyMoved[3] = true;
                    return true;                                            
                }
                
                return false; // if we arrive here it means we already checked for every possibility therefore the move isn't valid
        }

        // if we arrive here it means something went wrong with recognizing the piece id
        cout << "The piece you tried to move seemed to have a problem the program couldn't understand, try again or reboot the program. :(\n";
        return false; 
    }
    




}