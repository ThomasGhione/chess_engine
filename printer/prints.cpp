#include "chessengine.hpp"
#include "prints.hpp"

namespace print {

std::string Prints::getPlayer(const bool isWhiteTurn) {
    return (isWhiteTurn) ? "WHITE" : "BLACK";
}

std::string Prints::getPiece(const chess::Piece& piece) {
    if (piece.isWhite) {
        switch (piece.id) {
            case 1: return "P";
            case 2: return "N";
            case 3: return "B";
            case 4: return "R";
            case 5: return "Q";
            case 6: return "K";
            default: return "?";
        }
    }
    else {
        switch (piece.id) {
            case 1: return "p";
            case 2: return "n";
            case 3: return "b";
            case 4: return "r";
            case 5: return "q";
            case 6: return "k";
            default: return "?";
        }
    }
}

std::string Prints::getPieceOrEmpty(const chess::Board& board, const chess::coords& coords) {
    if (board.board[coords.file * 8 + coords.rank].id != EMPTY) {
        return getPiece(board.board[coords.file * 8 + coords.rank]);
    }
    return ((coords.rank + coords.file) % 2) ? "█" : " "; // default: case (EMPTY)
}

void Prints::getBoard(const chess::Board& board) { // white square unicode: \u2588

    #ifdef DEBUG
        auto start = chrono::steady_clock::now();
    #endif

    std::cout << "\n\n       A    B    C    D    E    F    G    H      LAST MOVE: "; // TODO: implement last move
    for (int rank = 7; rank >= 0; --rank) {
        if (rank % 2 == 1) {
            std::cout << "     █████     █████     █████     █████     \n" << rank + 1 << "    ";
            for (int file = 0; file < 8; file += 2)
                std::cout << "██" << getPieceOrEmpty(board, {file, rank}) << "██  " << getPieceOrEmpty(board, {file+1, rank}) << "  ";
            std::cout << "    " << rank+1 << "\n     █████     █████     █████     █████     \n";
        }
        else {
            std::cout << "          █████     █████     █████     █████\n" << rank + 1 << "    ";
            for (int file = 0; file < 8; file += 2)
                std::cout << "  " << getPieceOrEmpty(board, {rank, file}) << "  ██" << getPieceOrEmpty(board, {rank, file+1}) << "██";
            std::cout << "    " << rank + 1 << "\n          █████     █████     █████     █████\n";            
        }
        cout << "\n\n       A    B    C    D    E    F    G    H      TURN: " << board.getFullMoveClock() << "\n\n";
    }


/*
    cout << "\n\n       A    B    C    D    E    F    G    H      LAST MOVE: "
         << getPiece(gs.lastMove.id) << ' ' << gs.lastMove.file << gs.lastMove.rank << ' ' << gs.lastMove.movesTo.file << gs.lastMove.movesTo.rank << "\n\n\n";
    for (int rank = ML - 1; rank >= 0; --rank) {
        if (rank % 2 == 1) {
            cout << "     █████     █████     █████     █████     \n" << rank + 1 << "    ";
            for (int file = 0; file < ML; file += 2)
                cout << "██" << getPieceOrEmpty(gs.chessboard, rank, file) << "██  " << getPieceOrEmpty(gs.chessboard, rank, file+1) << "  ";
            cout << "    " << rank + 1 << "\n     █████     █████     █████     █████     \n";
        }
        else {
            cout << "          █████     █████     █████     █████\n" << rank + 1 << "    ";
            for (int file = 0; file < ML; file += 2)
                cout << "  " << getPieceOrEmpty(gs.chessboard, rank, file) << "  ██" << getPieceOrEmpty(gs.chessboard, rank, file+1) << "██";
            cout << "    " << rank + 1 << "\n          █████     █████     █████     █████\n";
        }
    }
    cout << "\n\n       A    B    C    D    E    F    G    H      TURN: " << gs.turns << "\n\n";
*/
    #ifdef DEBUG
        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        cout << "\nTime to print: " << chrono::duration <double, milli> (diff).count() << " ms\n";
    #endif
}

}
