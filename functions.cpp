#include "gamestatus.h"

// #pragma GCC push_options
// #pragma GCC optimize ("unroll-loops")

namespace chess {

    __attribute__((optimize("unroll-loops")))
    void startingPosition(gameStatus &gs) noexcept {
        // white pieces
        gs.chessboard[0][0].piece = gs.chessboard[0][7].piece = WHITE | ROOK;
        gs.chessboard[0][1].piece = gs.chessboard[0][6].piece = WHITE | KNIGHT;
        gs.chessboard[0][2].piece = gs.chessboard[0][5].piece = WHITE | BISHOP;
        gs.chessboard[0][3].piece = WHITE | QUEEN;
        gs.chessboard[0][4].piece = WHITE | KING;

        // black pieces
        gs.chessboard[7][0].piece = gs.chessboard[7][7].piece = BLACK | ROOK;
        gs.chessboard[7][1].piece = gs.chessboard[7][6].piece = BLACK | KNIGHT;
        gs.chessboard[7][2].piece = gs.chessboard[7][5].piece = BLACK | BISHOP;
        gs.chessboard[7][3].piece = BLACK | QUEEN;
        gs.chessboard[7][4].piece = BLACK | KING;

        // all pawns
        for (int file = 0; file < DIM; ++file) {
            gs.chessboard[1][file].piece = WHITE | PAWN;
            gs.chessboard[6][file].piece = BLACK | PAWN;
        }
        // empty squares are considered pieces, so we need to set up empty squares too 
        for (int rank = 2; rank < 6; ++rank)
            for (int file = 0; file < DIM; ++file)
                gs.chessboard[rank][file].piece = EMPTY;


        // set up whitePieces
        gs.whitePieces = { {{WHITE | ROOK, 'a', 1}}, {{WHITE | KNIGHT, 'b', 1}}, {{WHITE | BISHOP, 'c', 1}}, {{WHITE | QUEEN, 'd', 1}},
                           {{WHITE | KING, 'e', 1}}, {{WHITE | BISHOP, 'f', 1}}, {{WHITE | KNIGHT, 'g', 1}}, {{WHITE | ROOK, 'h', 1}},
                            {{WHITE | PAWN, 'a', 2}}, {{WHITE | PAWN, 'b', 2}}, {{WHITE | PAWN, 'c', 2}}, {{WHITE | PAWN, 'd', 2}},
                            {{WHITE | PAWN, 'e', 2}}, {{WHITE | PAWN, 'f', 2}}, {{WHITE | PAWN, 'g', 2}}, {{WHITE | PAWN, 'h', 2}} };
        // set up blackPieces
        gs.blackPieces = { {{BLACK | ROOK, 'a', 8}}, {{BLACK | KNIGHT, 'b', 8}}, {{BLACK | BISHOP, 'c', 8}}, {{BLACK | QUEEN, 'd', 8}},
                           {{BLACK | KING, 'e', 8}}, {{BLACK | BISHOP, 'f', 8}}, {{BLACK | KNIGHT, 'g', 8}}, {{BLACK | ROOK, 'h', 8}},
                            {{BLACK | PAWN, 'a', 7}}, {{BLACK | PAWN, 'b', 7}}, {{BLACK | PAWN, 'c', 7}}, {{BLACK | PAWN, 'd', 7}},
                            {{BLACK | PAWN, 'e', 7}}, {{BLACK | PAWN, 'f', 7}}, {{BLACK | PAWN, 'g', 7}}, {{BLACK | PAWN, 'h', 7}} };
    } 

    __attribute__((optimize("unroll-loops")))
    void createInitialBoard(gameStatus &gs) noexcept { // set up light squares
        for (int rank = 0; rank < DIM; ++rank)
            for (int file = 0; file < DIM; ++file) 
                (((rank + file) % 2)) ? (gs.chessboard[rank][file].isLightSquare = true) : (gs.chessboard[rank][file].isLightSquare = false);
        startingPosition(gs);
    }


    std::string printPiece(const board debugboard[DIM][DIM], const int rank, const int file) noexcept {
        if (debugboard[rank][file].piece != EMPTY)
            return getPiece(debugboard[rank][file].piece);
        return (debugboard[rank][file].isLightSquare)
            ? "█"
            : " "; // default: case (EMPTY)
    }

    __attribute__((optimize("unroll-loops")))
    void printBoard(gameStatus &gs) noexcept { // white square unicode: \u2588

        #ifdef DEBUG
            auto start = std::chrono::steady_clock::now();
        #endif

        std::cout << "\n\n       A    B    C    D    E    F    G    H      LAST MOVE: " << getPiece(gs.lastMove.piece) << ' ' << gs.lastMove.file1 << gs.lastMove.rank1 << ' ' <<gs.lastMove.file2 << gs.lastMove.rank2 << "\n\n\n";
        for (int rank = DIM - 1; rank >= 0; --rank) {
            if (gs.chessboard[rank][0].isLightSquare) {
                std::cout << "     █████     █████     █████     █████     \n" << rank + 1 << "    ";
                for (int file = 0; file < DIM; file += 2)
                    std::cout << "██" << printPiece(gs.chessboard, rank, file) << "██  " << printPiece(gs.chessboard, rank, file+1) << "  ";
                std::cout << "    " << rank + 1 << "\n     █████     █████     █████     █████     \n";
            } else {
                std::cout << "          █████     █████     █████     █████\n" << rank + 1 << "    ";
                for (int file = 0; file < DIM; file += 2)
                    std::cout << "  " << printPiece(gs.chessboard, rank, file) << "  ██" << printPiece(gs.chessboard, rank, file+1) << "██";
                std::cout << "    " << rank + 1 << "\n          █████     █████     █████     █████\n";
            }
        } std::cout << "\n\n       A    B    C    D    E    F    G    H      TURN: " << gs.turns << "\n\n";

        #ifdef DEBUG
            auto end = std::chrono::steady_clock::now();
            auto diff = end - start;
            std::cout << "\nTime to print: " << std::chrono::duration <double, std::milli> (diff).count() << " ms\n";
        #endif
    }

    // #pragma GCC pop_options

}