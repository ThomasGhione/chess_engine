#include "chessengine.h"

namespace chess {

    int fromCharToInt(char file) {
        switch (std::tolower(file)) {
            case 'a': return 1;
            case 'b': return 2;
            case 'c': return 3;
            case 'd': return 4;
            case 'e': return 5;
            case 'f': return 6;
            case 'g': return 7;
            case 'h': return 8;
            default: throw std::invalid_argument("invalid file coord");
        }
    }

    //void input move
    void inputMove(gameStatus &gs) {
        std::cout << "\nPress M/m for more infos, or input your move: ";
        char iFile1, iFile2;
        int iRank1, iRank2;

        ifWrongMove: // if something goes wrong while inputting the coords or the move isn't valid then it goes back here and ask for a valid move
        std::cin >> iFile1;
        if (iFile1 == 'M' || iFile1 == 'm') {
            std::cout << "MORE INFOS:\n    Format: file1rank1 file2rank2; example: e2 e4\n    Press M/m to print this menu again\n    Press Q/q to exit the program\nInput: ";
            goto ifWrongMove;
        }
        
        if (iFile1 == 'Q' || iFile1 == 'q') exit(EXIT_SUCCESS);   // if the first input is Q/q then exit the program successfully
        std::cin >> iRank1 >> iFile2 >> iRank2;                   // otherwise keep asking the rest of the input/coords

        if (iRank1 < 1 || iRank1 > 8 || iRank2 < 1 || iRank2 > 8 || iFile1 < 65 || (iFile1 > 72 && iFile1 < 97) || iFile1 > 104 || iFile2 < 65 || (iFile2 > 72 && iFile2 < 97) || iFile2 > 104) {
            std::cout << "Invalid coords! Choose again: ";
            goto ifWrongMove; // if coords are "< a1" || "> h8" then try again
        }

        if (gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece == EMPTY) {
            std::cout << "You can't choose an empty square! Choose again: ";
            goto ifWrongMove; // if square is empty then try again
        }
        
        switch (gs.player) {
            case WHITE:
                if (gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece > (unsigned char)(WHITE | KING)) { // check if input is valid (u can't choose your opponent pieces)
                    std::cout << "It's white's turn! Choose again: ";
                    goto ifWrongMove; // if selected square has opponent's pieces then try again
                } if (isMoveValid(gs, iRank1, fromCharToInt(iFile1), iRank2, fromCharToInt(iFile2))) {          // check if move is valid
                    ++gs.turns; // inc turn after checking if move is valid
                    gs.player = BLACK;
                    gs.lastMoveArray[0].file1 = iFile1;                                                         /********************/
                    gs.lastMoveArray[0].rank1 = iRank1;                                                         /* settings up      */
                    gs.lastMoveArray[0].file2 = iFile2;                                                         /* last move coords */
                    gs.lastMoveArray[0].rank2 = iRank2;                                                         /* and pieces       */
                    gs.lastMoveArray[0].piece = gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece;     /********************/
                    break;              
                } std::cout << "Move isn't valid! choose again: "; // if we arrived here then the move isn't valid, try again
                goto ifWrongMove;
            case BLACK:
                if (gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece < (unsigned char)BLACK) { // check if input is valid (u can't choose your opponent pieces)
                    std::cout << "It's black's turn! choose again: ";
                    goto ifWrongMove;
                } if (isMoveValid(gs, iRank1, fromCharToInt(iFile1), iRank2, fromCharToInt(iFile2))) {
                    gs.player = WHITE;
                    gs.lastMoveArray[1].file1 = iFile1;
                    gs.lastMoveArray[1].rank1 = iRank1;
                    gs.lastMoveArray[1].file2 = iFile2;
                    gs.lastMoveArray[1].rank2 = iRank2;
                    break;              
                } std::cout << "Move isn't valid! choose again: ";
                goto ifWrongMove;
            default: throw std::logic_error("logic_error"); // if player is neither white nor black then throw exception
        }

        //move the piece to the selected square and THEN delete the previous square piece
        gs.chessboard[iRank2 - 1][fromCharToInt(iFile2) - 1].piece = gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece;        
        gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece = EMPTY;
        debugprint(gs); //print the board every time a player makes a move
    }

    //start of the game
    char gameStarts() {
        std::cout << "MENU:\n    W: play as white\n    B: play as black\n    S: save (to add)\n    L: load last game (to add)\n    Q: quit\nINPUT: ";
        char option;
        ifWrongOption:
        std::cin >> option;
        switch (toupper(option)) {
            case 'W': return 'W';
            case 'B': return 'B';       //TODO - WILL BE FINISHED AFTER CREATING THE ENGINE PROTOTYPE
            case 'S': return 'S';       //TODO - IMPLEMENT SAVE
            case 'L': return 'L';       //TODO - IMPLEMENT LOAD
            case 'Q': exit(EXIT_SUCCESS);     
            default:
                std::cout << "invalid option! Please choose again: ";
                //std::cout << "player must be either white (W) or black (B), choose again: ";
                goto ifWrongOption;
        }
    }







}