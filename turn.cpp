#include "chessengine.h"

namespace chess {

    //void change turn
    unsigned int incTurn(unsigned int &turn, char &player) {
        return (player == 'W') ? ++turn : turn;
    }

    int fromCharToInt(char file) {
        file = std::tolower(file);
        switch (file) {
            case 'a': return 0;
            case 'b': return 1;
            case 'c': return 2;
            case 'd': return 3;
            case 'e': return 4;
            case 'f': return 5;
            case 'g': return 6;
            case 'h': return 7;
            default: throw std::invalid_argument("invalid file coord");
        }
    }


    //void input move
    void inputMove(gameStatus &gamestatus, char &player) {
        std::cout << "\nInput your move\nFormat: FILE1 RANK1 FILE2 RANK2\nOr press Q/q to quit\nInput: ";
        char iFile1, iFile2;
        int iRank1, iRank2;

        ifWrongMove: // if player chooses their opponent's pieces then go back to "ifWrongMove" to choose again
        std::cin >> iFile1;
        if (iFile1 == 'Q' || iFile1 == 'q') exit(0); // quit 
        std::cin >> iRank1 >> iFile2 >> iRank2;
        if (gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece == EMPTY) {
            std::cout << "You can't choose an empty square! Choose again: ";
            goto ifWrongMove; // if square is empty then try again
        }
        
        switch (player) {
            case 'W':
                if (gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece > (unsigned char)(WHITE | KING)) { //check if input is valid (u can't choose your opponent pieces)
                    std::cout << "It's white's turn! Choose again: ";
                    goto ifWrongMove; // if selected square has opponent's pieces then try again
                } 
                if (isMoveValid('W', gamestatus.chessboard,
                                iRank1 - 1, fromCharToInt(iFile1),
                                iRank2 - 1, fromCharToInt(iFile2))) {
                    incTurn(gamestatus.turns, player);
                    player = 'B';
                    break;              
                }
                std::cout << "Move isn't valid! choose again: ";
                goto ifWrongMove;
                

            case 'B':
                if (gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece < (unsigned char)BLACK) { //check if input is valid (u can't choose your opponent pieces)
                    std::cout << "It's black's turn! choose again: ";
                    goto ifWrongMove; // if selected square has opponent's pieces then try again
                }
                player = 'W';
                break;
            default: throw std::logic_error("logic_error"); // if player is neither white nor black then throw exception
        }
        std::cout << "debug fine switch" << std::endl;
        //move the piece to the selected square and THEN delete the previous square piece
        gamestatus.chessboard[iRank2 - 1][fromCharToInt(iFile2)].piece = gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece;        
        gamestatus.chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece = EMPTY;
        debugprint(gamestatus); //print the board every time a player makes a move
    }

    //start of the game
    char gameStarts() {
        std::cout << "MENU:\n    W: play as white\n    B: play as black\n    S: save (to add)\n    L: load last game (to add)\n    Q: quit\nINPUT: ";
        char player;
        ifWrongOption:
        std::cin >> player;
        switch (player) {
            case 'W': return 'W';
            case 'B': return 'B';       //TODO WILL BE FINISHED AFTER CREATING (AT LEAST) THE ENGINE PROTOTYPE
            case 'S': return 'S';       //TODO IMPLEMENT SAVE
            case 'L': return 'L';       //TODO IMPLEMENT LOAD
            case 'Q': exit(0);     
            default: 
                std::cout << "player must be either white (W) or black (B), choose again: ";
                goto ifWrongOption;
        }
    }







}