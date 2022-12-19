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

    std::string playerString(unsigned char &player) {
        return (player == WHITE) ? "white" : "black";
    }

    //void input move
    void inputMove(gameStatus &gs) {
        std::cout << "\nPress M/m for more infos, or input your move: ";
        char iFile1, iFile2;
        int iRank1, iRank2;

        ifWrongMove: // if something goes wrong while inputting the move or if it isn't valid then it goes back here and ask again until it's valid
        std::cin >> iFile1;
        if (iFile1 == 'M' || iFile1 == 'm') {
            std::cout << "MORE INFOS:\n    Format: file1rank1 file2rank2; example: e2 e4\n    Press M/m to print this menu again\n    Press Q/q to exit the program\nInput: ";
            goto ifWrongMove;
        }
        
        // check if the first input is equal to Q/q and if so quits
        if (iFile1 == 'Q' || iFile1 == 'q') exit(EXIT_SUCCESS);

        // if we arrive here we didn't exit, so we can ask the remaining inputs
        std::cin >> iRank1 >> iFile2 >> iRank2;

        //! DEBUG ONLY
        auto start = std::chrono::steady_clock::now();

        // check if coords are valid: "< a1" || "> h8"
        if (iRank1 < 1 || iRank1 > 8 || iRank2 < 1 || iRank2 > 8 || iFile1 < 65 || (iFile1 > 72 && iFile1 < 97) || iFile1 > 104 || iFile2 < 65 || (iFile2 > 72 && iFile2 < 97) || iFile2 > 104) {
            std::cout << "Invalid coords! Choose again: ";
            goto ifWrongMove;
        }

        // check if the starting square selected is empty
        if (gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece == EMPTY) {
            std::cout << "You can't choose an empty square! Choose again: ";
            goto ifWrongMove;
        }

        // check if we're selecting an opponent's piece
        if ((gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece & PLAYERMASK) != gs.player) {
            std::cout << "It's " << playerString(gs.player) << "'s turn! Choose again: ";
            goto ifWrongMove;
        }

        // check if it's a legal move
        if (!(isMoveValid(gs, iRank1, fromCharToInt(iFile1), iRank2, fromCharToInt(iFile2)))) {
            std::cout << "This move isn't legal! choose again: ";
            goto ifWrongMove;
        }

        // if we arrive here it means the move is valid, therefore we make the other player move and (if it's white we increment the turn counter)
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
        gs.lastMove.file1 = iFile1;
        gs.lastMove.rank1 = iRank1;
        gs.lastMove.file2 = iFile2;
        gs.lastMove.rank2 = iRank2;
        gs.lastMove.piece = gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece;    

        //move the piece to the selected square and THEN delete the previous square piece
        gs.chessboard[iRank2 - 1][fromCharToInt(iFile2) - 1].piece = gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece;        
        gs.chessboard[iRank1 - 1][fromCharToInt(iFile1) - 1].piece = EMPTY;

        //! DEBUG ONLY  
        auto end = std::chrono::steady_clock::now();
        auto diff = end - start;

        // print the chessboard after every move
        debugprint(gs);

        //! DEBUG ONLY
        std::cout << std::chrono::duration <double, std::nano> (diff).count() << " ns\n";
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
                std::cout << "invalid option! Please choose again: "; //std::cout << "player must be either white (W) or black (B), choose again: ";
                goto ifWrongOption;
        }
    }


}