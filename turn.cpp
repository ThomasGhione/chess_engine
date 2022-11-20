#include "chessengine.h"

namespace chess {

    //void change turn
    void incTurn() {

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
    void inputMove(board chessboard[ML][ML], char &player) {
        std::cout << "\nInput your move\nFormat: FILE1 RANK1 FILE2 RANK2\nOr press Q to quit\nInput: ";
        int iRank1, iRank2;
        char iFile1, iFile2;
        if (iFile1 == 'Q') throw std::invalid_argument("QUIT"); //TODO make better quit
        std::cin >> iFile1 >> iRank1 >> iFile2 >> iRank2;

        //check if input is valid (u can't choose your opponent pieces)
        switch (player) {
            case 'W':
                if (chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece > (unsigned char)(WHITE | KING))
                    throw std::invalid_argument("u chose ur opponent piece");
                player = 'B';
                break;
            case 'B':
                if (chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece < (unsigned char)BLACK)
                    throw std::invalid_argument("u chose ur opponent piece");
                player = 'W';
                break;
            default: throw std::logic_error("logic_error");
        }
        
        //move the piece to the selected square
        chessboard[iRank2 - 1][fromCharToInt(iFile2)].piece = chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece;        

        //delete the previous square piece
        chessboard[iRank1 - 1][fromCharToInt(iFile1)].piece = EMPTY;

        debugprint(chessboard);
    }

    //start of the game
    char gameStarts() {
        std::cout << "MENU:\n    W: play as white\n    B: play as black\n    S: save (to add)\n    L: load last game (to add)\n    Q: quit\nINPUT: ";
        char player;
        std::cin >> player;
        switch (player) {
            case 'W': return 'W';
            case 'B': return 'B';                         //TODO WILL BE FINISHED AFTER CREATING (AT LEAST) THE ENGINE PROTOTYPE
            case 'S': return 'S';                         //TODO IMPLEMENT SAVE
            case 'L': return 'L';                         //TODO IMPLEMENT LOAD
            case 'Q': throw std::logic_error("quit");     //TODO MAKE BETTER QUIT
            default: throw std::invalid_argument("player must be either white (W) or black (B)");
        }
    }







}