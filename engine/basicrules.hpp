#ifndef ENGINE_BASICRULES_HPP
#define ENGINE_BASICRULES_HPP

#include <cstdint>

#include "basebonuspenaltyvalues.hpp"

namespace engine {

    // implement B > 3P where B=bishop, P=pawn

    static constexpr int64_t BISHOP_VALUE = 330;
    static constexpr int64_t PAWN_VALUE = 100;

/*
 *   Avoid exchanging one minor piece for three pawns.
 *   B > 3P
 *   N > 3P
 *  (for simplicity, we treat bishops and knights equally here)
 */

    // Returns a penalty score for unfavorable exchanges
    //TODO TEST THIS
    int64_t avoidUnfavorableExchanges(int64_t bishopCount, int64_t knightCount, int64_t pawnCount) {
        int64_t minorPieceCount = bishopCount + knightCount;
        int64_t unfavorableExchanges = 0;

        while (minorPieceCount >= 1 && pawnCount >= 3) {
            unfavorableExchanges++;
            minorPieceCount--;
            pawnCount -= 3;
        }

        return unfavorableExchanges * (BISHOP_VALUE - 3 * PAWN_VALUE);
    }
 



/*
 *
 * Encourage the engine to have the bishop pair.
 * B > N
 * 
 */

    int64_t bonusBishopPair(int64_t bishopCount, int64_t knightCount) {
        if (bishopCount == 2) {
            return 15;
        } else {
            return 0;
        }

    }

}

#endif // ENGINE_BASICRULES_HPP