#include "chessengine.h"

namespace chess {

    //void input move
    void inputMove(gameStatus &gs) noexcept {
        cout << "\nPress M/m for more infos, or input your move: ";
        char iFile1, iFile2;
        int iRank1, iRank2;

        ifWrongMove: // if something goes wrong while inputting the move or if it isn't valid then it goes back here and ask again until it's valid
        cin >> iFile1;

        if (iFile1 == 'M' || iFile1 == 'm') {
            cout << "MORE INFOS:\n    Format: file1rank1 file2rank2; example: e2 e4\n    Press M/m to print this menu again\n    Press Q/q to exit the program\n    Press P/p to print every move\nInput: ";
            // goto ifWrongMove;
        }

        //TODO fix
        // if user inputs P/p then print all moves
        /*
        if (iFile1 == 'P' || iFile1 == 'p') {
            printAllMoves(gs.listOfMoves);
            goto ifWrongMove;
        }
        */

        // check if the first input is equal to Q/q and if so quits
        if (iFile1 == 'Q' || iFile1 == 'q')
            exit(EXIT_SUCCESS);

        // if we arrive here we didn't exit, so we can ask the remaining inputs
        cin >> iRank1 >> iFile2 >> iRank2;

        #ifdef DEBUG
            auto start = chrono::steady_clock::now();
        #endif

        // check for specific errors, due to the input being invalid or meaningless
        // check if coords are valid: "< a1" || "> h8"
        if (iRank1 < 1 || iRank1 > 8 || iRank2 < 1 || iRank2 > 8 || iFile1 < 65 || (iFile1 > 72 && iFile1 < 97) || iFile1 > 104 || iFile2 < 65 || (iFile2 > 72 && iFile2 < 97) || iFile2 > 104) {
            cout << "Invalid coords! Choose again: ";
            iFile1 = iFile2 = '0';
            iRank1 = iRank2 = 0;
            goto ifWrongMove;
        }
        // check if the starting square selected is empty
        if (!gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece.id) {
            cout << "You can't choose an EMPTY square! Choose again: ";
            goto ifWrongMove;
        }
        // check if we're selecting an opponent's piece
        if ((gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece.id & PLAYERMASK) != gs.player) {
            cout << "It's " << playerString(gs.player) << "'s turn! Choose again: ";
            goto ifWrongMove;
        }
        // you can't take your own pieces
        if ((gs.chessboard[iRank2 - 1][fromCharToInt(iFile2) - 1].piece.id & PLAYERMASK) == gs.player) {
            cout << "You can't take your pieces! Choose again: ";
            goto ifWrongMove;
        }        

        // check if it's a legal move
        // isMoveValid checks if a move is legal according to the chess rules
        if (!isMoveValid(gs, iRank1, fromCharToInt(iFile1), iRank2, fromCharToInt(iFile2))) {
            cout << "This move isn't legal! Choose again: ";
            goto ifWrongMove;
        }



        // update wherePieceAt
        //TODO DEBUG ONLY:
        //printPieceCoordsV(gs.wherePieceAt);

        // if we arrive here it means the move is valid, therefore we make the other player move (and if it's white we increment the turn counter)
        switch (gs.player) {
            case WHITE:
                ++gs.turns;
                gs.player = BLACK;
                break;
            case BLACK:
                gs.player = WHITE;
                break;
        }
        
        // update lastMove 
        gs.lastMove.piece.coords.file = iFile1;
        gs.lastMove.piece.coords.rank = iRank1;
        gs.lastMove.movesTo.file = iFile2;
        gs.lastMove.movesTo.rank = iRank2;
        gs.lastMove.piece.id = gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece.id;

/*
        gs.lastMove.file1 = iFile1;
        gs.lastMove.rank1 = iRank1;
        gs.lastMove.file2 = iFile2;
        gs.lastMove.rank2 = iRank2;
        gs.lastMove.piece = gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece;   
*/
        // update listOfMoves
        gs.listOfMoves.push_back(gs.lastMove);

        //move the piece to the selected square and THEN delete the previous square piece
        gs.chessboard[iRank2 - 1][fromCharToInt(iFile2) - 1].piece = gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece;        
        gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece.id = EMPTY;

        #ifdef DEBUG
            auto end = chrono::steady_clock::now();
            auto diff = end - start;
        #endif

        printBoard(gs); // print the chessboard after every move

        #ifdef DEBUG
            cout << "Time to calculate if the move was legal: " << chrono::duration <double, nano> (diff).count() << " ns\n";
        #endif
    }

    //start of the game
    char gameStarts() noexcept {
        cout << "MENU:\n    W: play as white\n    B: play as black\n    S: save (to add)\n    L: load last game (to add)\n    Q: quit\nINPUT: ";
        char option;
        cin >> option;

        if (option == 'q' || option == 'Q')
            exit(EXIT_SUCCESS);

        return toupper(option);
    }


}