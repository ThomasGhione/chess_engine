#pragma once

// Dual-perspective NNUE accumulator: v[0] = white's view, v[1] = black's view.
//
// Lives on Board next to the other incremental eval fields. It is updated by
// Board's addPieceToBB/removePieceFromBB (every doMove/undoMove piece event
// flows through them), and add/sub are exact integer inverses, so undoMove
// needs no MoveState snapshot. Board rebuilds (FEN load, table refresh) call
// Board::refreshNnueAccumulator() for a from-scratch recompute.

#include <cstdint>
#include <cstring>

#include "network.hpp"

namespace NNUE {

struct alignas(64) Accumulator {
    int16_t v[2][HIDDEN];

    inline void reset() noexcept {
        const Network& net = *activeNetwork;
        std::memcpy(v[0], net.featureBias, sizeof(net.featureBias));
        std::memcpy(v[1], net.featureBias, sizeof(net.featureBias));
    }

    // piece = Board nibble (WHITE=0x8 color bit | type 1..6), index = engine
    // square (0 = a8, 63 = h1); LERF square = index ^ 56.
    // Feature from perspective X: isOpp*384 + type*64 + sqFromX, with sqFromX
    // vertically mirrored for Black (bullet Chess768 convention).
    template<bool Add>
    inline void update(uint8_t piece, uint8_t index) noexcept {
        const Network& net = *activeNetwork;
        const int type = (piece & 0x7) - 1;      // P..K -> 0..5
        const bool black = (piece & 0x8) == 0;   // Board::WHITE = 0x8
        const int lerf = index ^ 56;
        const int featWhiteView = (black ? 384 : 0) + type * 64 + lerf;
        const int featBlackView = (black ? 0 : 384) + type * 64 + (lerf ^ 56);
        const int16_t* __restrict wW = net.featureWeights[featWhiteView];
        const int16_t* __restrict wB = net.featureWeights[featBlackView];
        for (int i = 0; i < HIDDEN; ++i) {
            if constexpr (Add) {
                v[0][i] = static_cast<int16_t>(v[0][i] + wW[i]);
                v[1][i] = static_cast<int16_t>(v[1][i] + wB[i]);
            } else {
                v[0][i] = static_cast<int16_t>(v[0][i] - wW[i]);
                v[1][i] = static_cast<int16_t>(v[1][i] - wB[i]);
            }
        }
    }
};

} // namespace NNUE
