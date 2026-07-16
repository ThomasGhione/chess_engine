#pragma once

// Dual-perspective NNUE accumulator: v[0] = white's view, v[1] = black's view.
//
// HalfKA king buckets (HALFKA_PLAN.md): every feature index of perspective X
// depends on the (bucket, flip) of X's OWN king — the "basis". The basis of
// each perspective is cached here (base = 768*bucket, flip), and the class
// keeps this invariant:
//
//   clean[p]  =>  every feature in v[p] was accumulated with the stored
//                 basis, and that basis matches the current own-king square.
//
// Piece events (Board's addPieceToBB/removePieceFromBB) update both
// perspectives incrementally under their stored bases; add/sub are exact
// integer inverses, so undoMove needs no MoveState snapshot. When a king
// ADD lands on a square whose basis differs from the stored one, that
// perspective is marked DIRTY and stops applying updates: its features
// would need the new basis for the whole board, not a delta. The lazy
// refresh happens in Board::ensureNnueAccumulatorClean() (called by
// NNUE::evaluate and the selftest), where the Board is consistent —
// mid-doMove refreshes would read a half-updated position. Undo needs no
// special case: while dirty everything is skipped and the next refresh
// rebuilds from the real board; while clean the inverse deltas are exact.
//
// Board rebuilds (FEN load, table refresh) call refreshNnueAccumulator()
// for a from-scratch recompute of both perspectives.

#include <cstdint>
#include <cstring>

#include "network.hpp"

namespace NNUE {

struct alignas(64) Accumulator {
    int16_t v[2][HIDDEN];
    int32_t base[2];   // 768 * king bucket, per perspective
    uint8_t flip[2];   // 0 or 7 (file mirror), per perspective
    bool    dirty[2];  // true = v[p] stale, skip updates until refresh

    // Wipes both perspectives to the bias and installs the given own-king
    // squares (LERF, already from each perspective's view) as bases.
    inline void resetWithKings(int wKingLerf, int bKingLerfFromBlack) noexcept {
        const Network& net = *activeNetwork;
        std::memcpy(v[0], net.featureBias, sizeof(net.featureBias));
        std::memcpy(v[1], net.featureBias, sizeof(net.featureBias));
        base[0] = kingFeatureBase(wKingLerf);
        base[1] = kingFeatureBase(bKingLerfFromBlack);
        flip[0] = static_cast<uint8_t>(kingFlip(wKingLerf));
        flip[1] = static_cast<uint8_t>(kingFlip(bKingLerfFromBlack));
        dirty[0] = dirty[1] = false;
    }

    // Rebuild helpers for a single perspective (lazy refresh path).
    inline void resetPerspective(int p, int ownKingLerfView) noexcept {
        const Network& net = *activeNetwork;
        std::memcpy(v[p], net.featureBias, sizeof(net.featureBias));
        base[p] = kingFeatureBase(ownKingLerfView);
        flip[p] = static_cast<uint8_t>(kingFlip(ownKingLerfView));
        dirty[p] = false;
    }

    template<bool Add>
    inline void updatePerspective(int p, uint8_t piece, uint8_t index) noexcept {
        const Network& net = *activeNetwork;
        const int type = (piece & 0x7) - 1;      // P..K -> 0..5
        const bool black = (piece & 0x8) == 0;   // Board::WHITE = 0x8
        const int lerf = index ^ 56;
        const int sqView = (p == 1) ? (lerf ^ 56) : lerf;
        const bool isOpp = (p == 1) ? !black : black;
        const int feat768 = (isOpp ? 384 : 0) + type * 64 + sqView;
        const int16_t* __restrict w = net.featureWeights[base[p] + (feat768 ^ flip[p])];
        for (int i = 0; i < HIDDEN; ++i) {
            if constexpr (Add) v[p][i] = static_cast<int16_t>(v[p][i] + w[i]);
            else               v[p][i] = static_cast<int16_t>(v[p][i] - w[i]);
        }
    }

    // piece = Board nibble (WHITE=0x8 color bit | type 1..6), index = engine
    // square (0 = a8, 63 = h1); LERF square = index ^ 56.
    template<bool Add>
    inline void update(uint8_t piece, uint8_t index) noexcept {
        const int type = (piece & 0x7) - 1;
        const bool black = (piece & 0x8) == 0;
        const int lerf = index ^ 56;

        for (int p = 0; p < 2; ++p) {
            if (dirty[p]) continue;
            // A king ADD of this perspective's own king that lands on a
            // square with a different basis invalidates every feature of
            // this perspective: mark dirty, the next ensureClean rebuilds.
            // (The matching REMOVE is always applied under the stored basis,
            // which by the invariant is the basis the king was added with.)
            if constexpr (Add) {
                if (type == 5 && black == (p == 1)) {
                    const int kView = (p == 1) ? (lerf ^ 56) : lerf;
                    if (kingFeatureBase(kView) != base[p]
                        || kingFlip(kView) != flip[p]) {
                        dirty[p] = true;
                        continue;
                    }
                }
            }
            updatePerspective<Add>(p, piece, index);
        }
    }
};

} // namespace NNUE
