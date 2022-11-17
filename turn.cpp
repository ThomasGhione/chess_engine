#include "chessengine.h"

namespace chess {

    //void change turn
    void incTurn() {

    }



    //void input move
    void inputMove(board chessboard[ML][ML], char &player) {
        std::cout << "input your move: ";
        int iRank1, iFile1, iRank2, iFile2;
        std::cin >> iRank1 >> iFile1 >> iRank2 >> iFile2;

        //check if input is valid (u can't choose your opponent pieces)
        switch (player) {
            case 'W':
                if (chessboard[iFile1][iRank1].piece < WHITE)
                    throw std::invalid_argument("u chose ur opponent piece");
                player = 'B';
                break;
            case 'B':
                if (chessboard[iFile1][iRank1].piece >= WHITE)
                    throw std::invalid_argument("u chose ur opponent piece");
                player = 'W';
                break;
            default: throw std::logic_error("logic_error");
        }
        
        //move the piece to the selected square
        chessboard[iFile2][iRank2].piece = chessboard[iFile1][iRank1].piece;        

        //delete the previous square piece
        chessboard[iFile1][iRank1].piece = EMPTY;

        debugprint(chessboard);
    }

    //start of the game
    char gameStarts(board chessboard[ML][ML]) {
        std::cout << "MENU:\n    W: play as white\n    B: play as black\n    S: save (to add)\n    L: load last game (to add)\n    Q: quit\nINPUT: ";
        char player;
        std::cin >> player;
        switch (player) {
            case 'W': return 'W';
            case 'B': return 'B';
            case 'S': return 'S'; //TODO IMPLEMENT SAVE
            case 'L': return 'L'; //TODO IMPLEMENT LOAD
            case 'Q': throw std::logic_error("quit");
            default: throw std::invalid_argument("player must be either white (W) or black (B)");
        }
    }







}