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
    void inputMove(gameStatus &gamestatus, unsigned char &player) {
        std::cout << "\nInput your move\nFormat: FILE1 RANK1 FILE2 RANK2\nOr press Q/q to quit\nInput: ";
        char iFile1, iFile2;
        int iRank1, iRank2;

        ifWrongMove: // if player chooses their opponent's pieces then go back to "ifWrongMove" to choose again
        std::cin >> iFile1;
        if (iFile1 == 'Q' || iFile1 == 'q') exit(EXIT_SUCCESS); // quit 
        std::cin >> iRank1 >> iFile2 >> iRank2;

        if (iRank1 < 1 || iRank1 > 8 || iRank2 < 1 || iRank2 > 8 ||
            iFile1 < 65 || (iFile1 > 72 && iFile1 < 97) || iFile1 > 104 ||
            iFile2 < 65 || (iFile2 > 72 && iFile2 < 97) || iFile2 > 104) {
            std::cout << "Invalid coords! Choose again: ";
            goto ifWrongMove; // if coords have are "< a 1" || "> h 8" then they try again
        }

        if (gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece == EMPTY) {
            std::cout << "You can't choose an empty square! Choose again: ";
            goto ifWrongMove; // if square is empty then try again
        }
        
        switch (player) {
            case WHITE:
                if (gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece > (unsigned char)(WHITE | KING)) { //check if input is valid (u can't choose your opponent pieces)
                    std::cout << "It's white's turn! Choose again: ";
                    goto ifWrongMove; // if selected square has opponent's pieces then try again
                } 
                if (isMoveValid(WHITE, gamestatus.chessboard,
                                iRank1, fromCharToInt(iFile1),
                                iRank2, fromCharToInt(iFile2))) {
                    ++gamestatus.turns; //inc turn after checking if move is valid
                    player = BLACK;
                    break;              
                }
                std::cout << "Move isn't valid! choose again: ";
                goto ifWrongMove;
                

            case BLACK:
                if (gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece < (unsigned char)BLACK) { //check if input is valid (u can't choose your opponent pieces)
                    std::cout << "It's black's turn! choose again: ";
                    goto ifWrongMove; // if selected square has opponent's pieces then try again
                }
                if (isMoveValid(BLACK, gamestatus.chessboard,
                                iRank1, fromCharToInt(iFile1),
                                iRank2, fromCharToInt(iFile2))) {
                    player = WHITE;
                    break;              
                }
                std::cout << "Move isn't valid! choose again: ";
                goto ifWrongMove;
            default: throw std::logic_error("logic_error"); // if player is neither white nor black then throw exception
        }

        //move the piece to the selected square and THEN delete the previous square piece
        gamestatus.chessboard[iRank2 - 1][fromCharToInt(iFile2) - 1].piece = gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece;        
        gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece = EMPTY;
        debugprint(gamestatus); //print the board every time a player makes a move
    }

    //start of the game
    char gameStarts() {
        std::cout << "MENU:\n    W: play as white\n    B: play as black\n    S: save (to add)\n    L: load last game (to add)\n    Q: quit\nINPUT: ";
        char option;
        ifWrongOption:
        std::cin >> option;
        switch (toupper(option)) {
            case 'W': return 'W';
            case 'B': return 'B';       //TODO WILL BE FINISHED AFTER CREATING (AT LEAST) THE ENGINE PROTOTYPE
            case 'S': return 'S';       //TODO IMPLEMENT SAVE
            case 'L': return 'L';       //TODO IMPLEMENT LOAD
            case 'Q': exit(EXIT_SUCCESS);     
            default:
                std::cout << "invalid option! Please choose again: ";
                //std::cout << "player must be either white (W) or black (B), choose again: ";
                goto ifWrongOption;
        }
    }







}