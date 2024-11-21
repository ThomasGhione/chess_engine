#include "gamestatus.h"


namespace chess {

    void printPiecesCoords(const PiecesAndTheirLegalMoves& pieces) {
        move pieceAndCoords;
        for (unsigned int i = 0; i < pieces.size(); ++i) {
            pieceAndCoords = pieces[i].front();
            std::cout << getPiece(pieceAndCoords.piece) << ' ' << pieceAndCoords.file1 << pieceAndCoords.rank1 << '\n';
        }
    }

    void printAllPieces(const gameStatus& gs) {
        std::cout << "WHITE PIECES COORDS: \n";
        printPiecesCoords(gs.whitePieces);
        std::cout << "BLACK PIECES COORDS: \n";
        printPiecesCoords(gs.blackPieces);
    }

    void updatePieceCoords(PiecesAndTheirLegalMoves& pieces, const move& move) {
        // find piece
        for (unsigned int i = 0; i < pieces.size(); ++i) {
            if (pieces[i].front().piece == move.piece && pieces[i].front().file1 == move.file1 && pieces[i].front().rank1 == move.rank1) {
                // updates move
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
        // finds piece
        for (unsigned int i = 0; i < pieces.size(); ++i) {
            if (pieces[i].front().piece == pieceToDelete) {
                // deletes piece
                swap(pieces[i], pieces[pieces.size() - 1]);
                pieces.pop_back();
                break;
            }
        }
    }
}
