/*
#include "gamestatus.h"

namespace chess {

    struct checkStruct {
        static inline bool flag = false;
        static inline piece piece[2];
    };

    struct isCovered {
        bool whiteTerritory[DIM][DIM];
        bool blackTerritory[DIM][DIM];
    };


    // TODO: every time a player moes, we need to check again where the opponent's pieces can move 


    bool isInCheck() {
        return true;
    }

    void checkTerritory(board chessboard[DIM][DIM]) {
        for (int rank = 0; rank < DIM; ++rank) {
            for (int file = 0; file < DIM; ++file) {

                if ((chessboard[rank][file].piece && PLAYERMASK) == EMPTY) {
                    continue;
                    //! nothing to do here, skip to the next square
                }

                if ((chessboard[rank][file].piece && PLAYERMASK) == WHITE) {
                    //TODO check whiteTerritory
                }

                if ((chessboard[rank][file].piece && PLAYERMASK) == BLACK) {
                    //TODO check blackTerritory
                }

            }
        }
    }
}
*/