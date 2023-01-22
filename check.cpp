/*
#include "chessengine.h"

namespace chess {

    struct checkStruct {
        static inline bool flag = false;
        static inline piece_id piece[2];
    };

    struct isCovered {
        bool whiteTerritory[ML][ML];
        bool blackTerritory[ML][ML];
    };


    // TODO: every time a player moes, we need to check again where the opponent's pieces can move 


    bool isInCheck() {
        return true;
    }

    void checkTerritory(board chessboard[ML][ML]) {
        for (int rank = 0; rank < ML; ++rank) {
            for (int file = 0; file < ML; ++file) {

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