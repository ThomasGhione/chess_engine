#include "gamestatus.h"


namespace chess {

    void printAllPiecesCoords(const PiecesAndTheirLegalMoves& pieces) {
        move pieceAndCoords;
        for (int i = 0; i < pieces.size(); ++i) {
            pieceAndCoords = pieces[i].front();
            std::cout << pieceAndCoords.piece << ' ' << pieceAndCoords.file1 << pieceAndCoords.rank1 << '\n';
        }
    }


    void printAllPieces(gameStatus& gs) {
        std::cout << "WHITE PIECES COORDS: \n";
        printAllPiecesCoords(gs.whitePieces);
        std::cout << "BLACK PIECES COORDS: \n";
        printAllPiecesCoords(gs.blackPieces);
    }

    void updatePieceCoords(PiecesAndTheirLegalMoves& pieces, move move) {
        // find piece
        for (int i = 0; i < pieces.size(); ++i) {
            if (pieces[i].front().piece == move.piece && pieces[i].front().file1 == move.file1 && pieces[i].front().rank1 == move.rank1) {
                // update move
                pieces[i].front().file1 = move.file2;
                pieces[i].front().rank1 = move.rank2;
                break;
            }
        }
        //TODO UPDATE ITS LEGAL MOVES
    }

    void addPiece(PiecesAndTheirLegalMoves& pieces, move thisMove) {
        std::list<move> newList = { thisMove };
        pieces.push_back(newList);   
    }

    void deletePiece(PiecesAndTheirLegalMoves& pieces, piece pieceToDelete) {
        // find piece
        for (int i = 0; i < pieces.size(); ++i) {
            if (pieces[i].front().piece == pieceToDelete) {
                // delete piece
                swap(pieces[i], pieces[pieces.size() - 1]);
                pieces.pop_back();
                break;
            }
        }
    }
}
