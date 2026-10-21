#pragma once

// Dual-perspective NNUE accumulator: v[0] = white's view, v[1] = black's view.
//
// HalfKA king buckets: every feature index of perspective X
// depends on the (bucket, flip) of X's OWN king - the "basis". The basis of
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
// NNUE::evaluate and the selftest), where the Board is consistent -
// mid-doMove refreshes would read a half-updated position. Undo needs no
// special case: while dirty everything is skipped and the next refresh
// rebuilds from the real board; while clean the inverse deltas are exact.
//
// Board rebuilds (FEN load, table refresh) call refreshNnueAccumulator()
// for a from-scratch recompute of both perspectives.

#include <cstdint>
#include <cstring>

#include "network.hpp"
#include "network_deep.hpp"

namespace NNUE {

// One deferred accumulator mutation. doMove records these instead of touching
// the 1024-wide rows; evaluate replays them in order, which reproduces the
// eager sequence exactly (the dirty/basis transitions depend on that order).
// A do/undo pair that never reaches an evaluate cancels here and costs nothing.
struct AccDelta {
    enum Kind : uint8_t { Add, Remove, Move };
    uint8_t kind;
    uint8_t piece;
    uint8_t from;   // Add/Remove: the square
    uint8_t to;     // Move only
    bool operator==(const AccDelta&) const = default;
};

// Deepest chain of moves that can be made without an evaluate is bounded by
// MAX_PLY + the qsearch cap, at most three deltas each. The queue overflowing
// only forces an early replay, so correctness never rests on this number.
inline constexpr int MAX_ACC_PENDING = 512;

struct alignas(64) Accumulator {
    int16_t v[2][HIDDEN];
    int32_t base[2];   // 768 * king bucket, per perspective
    uint8_t flip[2];   // 0 or 7 (file mirror), per perspective
    bool    dirty[2];  // true = v[p] stale, skip updates until refresh

    // Wipes both perspectives to the bias and installs the given own-king
    // squares (LERF, already from each perspective's view) as bases.
    inline void resetWithKings(int wKingLerf, int bKingLerfFromBlack) noexcept {
        const Deep::NetworkDeep& net = *activeNetwork;
        std::memcpy(v[0], net.l0b, sizeof(net.l0b));
        std::memcpy(v[1], net.l0b, sizeof(net.l0b));
        base[0] = kingFeatureBase(wKingLerf);
        base[1] = kingFeatureBase(bKingLerfFromBlack);
        flip[0] = static_cast<uint8_t>(kingFlip(wKingLerf));
        flip[1] = static_cast<uint8_t>(kingFlip(bKingLerfFromBlack));
        dirty[0] = dirty[1] = false;
    }

    // A pair of queued deltas folded into one traversal of the rows. The queue
    // at a flush is almost always the previous sibling's move undone followed by
    // this move done, so replaying entry by entry reads and writes both rows
    // twice. int16 is modular, so the order the contributions are summed in
    // does not change the result.
    template<int NS, int NA>
    inline void updateFused(const int16_t* const (&sub)[2][2],
                            const int16_t* const (&add)[2][2]) noexcept {
        fusePair<NS, NA>(v[0], v[1], sub, add);
    }

    // The accumulator rows never alias the weight rows. Restrict-qualified
    // parameters say so: without them GCC versions the loop on runtime alias
    // checks at -O3 and leaves it scalar at -O2.
    template<int NS, int NA>
    static inline void fusePair(int16_t* __restrict v0, int16_t* __restrict v1,
                                const int16_t* const (&sub)[2][2],
                                const int16_t* const (&add)[2][2]) noexcept {
        for (int i = 0; i < HIDDEN; ++i) {
            int s0 = v0[i];
            int s1 = v1[i];
            for (int k = 0; k < NS; ++k) { s0 -= sub[0][k][i]; s1 -= sub[1][k][i]; }
            for (int k = 0; k < NA; ++k) { s0 += add[0][k][i]; s1 += add[1][k][i]; }
            v0[i] = static_cast<int16_t>(s0);
            v1[i] = static_cast<int16_t>(s1);
        }
    }

    // Weight row for one piece feature under an explicit basis: the live
    // perspectives use their own, the Finny cache rows hold a different one.
    [[nodiscard]] static inline const int16_t* rowFor(int rowBase, int rowFlip, int p,
                                                      uint8_t piece, uint8_t index) noexcept {
        const int type = (piece & 0x7) - 1;      // P..K -> 0..5
        const bool black = (piece & 0x8) == 0;   // Board::WHITE = 0x8
        const int lerf = index ^ 56;
        const int sqView = (p == 1) ? (lerf ^ 56) : lerf;
        const bool isOpp = (p == 1) ? !black : black;
        const int feat768 = (isOpp ? 384 : 0) + type * 64 + sqView;
        return activeNetwork->l0w[rowBase + (feat768 ^ rowFlip)];
    }

    [[nodiscard]] inline const int16_t* featureRow(int p, uint8_t piece, uint8_t index) const noexcept {
        return rowFor(base[p], flip[p], p, piece, index);
    }

    // Single-row counterpart of updateFused, for the Finny refresh.
    template<int NS, int NA>
    static inline void fuseRows(int16_t* __restrict row, const int16_t* const* sub,
                                const int16_t* const* add) noexcept {
        for (int i = 0; i < HIDDEN; ++i) {
            int x = row[i];
            for (int k = 0; k < NS; ++k) x -= sub[k][i];
            for (int k = 0; k < NA; ++k) x += add[k][i];
            row[i] = static_cast<int16_t>(x);
        }
    }

    // Walks a diff of ns subs and na adds in passes of at most two of each,
    // calling f.template operator()<S, A>(si, ai) on each pass. The counts are
    // template parameters so the kernels unroll: runtime bounds measured 50%
    // slower than replaying the deltas one at a time.
    template<class F>
    static inline void forEachPass(int ns, int na, F&& f) noexcept {
        for (int si = 0, ai = 0; si < ns || ai < na; ) {
            const int s = (ns - si > 2) ? 2 : ns - si;
            const int a = (na - ai > 2) ? 2 : na - ai;
            switch (s * 4 + a) {
                case 2 * 4 + 2: f.template operator()<2, 2>(si, ai); break;
                case 2 * 4 + 1: f.template operator()<2, 1>(si, ai); break;
                case 1 * 4 + 2: f.template operator()<1, 2>(si, ai); break;
                case 1 * 4 + 1: f.template operator()<1, 1>(si, ai); break;
                case 2 * 4 + 0: f.template operator()<2, 0>(si, ai); break;
                case 0 * 4 + 2: f.template operator()<0, 2>(si, ai); break;
                case 1 * 4 + 0: f.template operator()<1, 0>(si, ai); break;
                default:        f.template operator()<0, 1>(si, ai); break;
            }
            si += s;
            ai += a;
        }
    }

    // Applies a whole feature diff to one row. A refresh changes 5.8 features on
    // average, so this reads and writes the row about twice instead of six times.
    static inline void applyRows(int16_t* __restrict row,
                                 const int16_t* const* sub, int ns,
                                 const int16_t* const* add, int na) noexcept {
        forEachPass(ns, na, [&]<int S, int A>(int si, int ai) { fuseRows<S, A>(row, sub + si, add + ai); });
    }

    // piece = Board nibble (WHITE=0x8 color bit | type 1..6), index = engine
    // square (0 = a8, 63 = h1).
    template<bool Add>
    inline void update(uint8_t piece, uint8_t index) noexcept {
        // A king landing on a square with another basis invalidates every
        // feature of its own perspective: mark it dirty for the next refresh.
        // The matching remove is applied under the stored basis, the one the
        // king was added with.
        if (Add && (piece & 0x7) == 6) [[unlikely]] {
            const int p = (piece & 0x8) ? 0 : 1;
            const int kView = (p == 1) ? index : (index ^ 56);
            dirty[p] |= kingFeatureBase(kView) != base[p] || kingFlip(kView) != flip[p];
        }
        const int16_t* rows[2][2] = {{featureRow(0, piece, index)}, {featureRow(1, piece, index)}};
        if (!dirty[0] && !dirty[1]) [[likely]] {
            updateFused<!Add, Add>(rows, rows);
            return;
        }
        for (int p = 0; p < 2; ++p) {
            if (!dirty[p]) fuseRows<!Add, Add>(v[p], rows[p], rows[p]);
        }
    }

    // A quiet move in one pass over the rows instead of a remove and an add. A
    // king may change bucket, and a dirty perspective skips updates: both take
    // the two-step path.
    inline void updateMove(uint8_t piece, uint8_t fromIndex, uint8_t toIndex) noexcept {
        if ((piece & 0x7) == 6 || dirty[0] || dirty[1]) [[unlikely]] {
            update<false>(piece, fromIndex);
            update<true>(piece, toIndex);
            return;
        }
        const int16_t* sub[2][2] = {{featureRow(0, piece, fromIndex)}, {featureRow(1, piece, fromIndex)}};
        const int16_t* add[2][2] = {{featureRow(0, piece, toIndex)}, {featureRow(1, piece, toIndex)}};
        updateFused<1, 1>(sub, add);
    }
};

// Finny table: one cached accumulator row per (perspective, king bucket,
// mirror flip) plus the piece bitboards it was computed from. A lazy refresh
// becomes a DIFF against the cached state (typically a handful of piece
// updates) instead of a ~30-piece rebuild. Any cached content is a correct
// starting point as long as the entry's basis matches - even from another
// game - so the table lives per-thread (Lazy SMP helpers each get their own)
// and never needs invalidation. The bias-initialised entry with empty
// bitboards is itself a valid state ("diff from the empty board").
struct FinnyEntry {
    alignas(64) int16_t v[HIDDEN];
    uint64_t bb[2][6]; // [colorIndex][type-1], engine bit convention
};

struct FinnyTable {
    FinnyEntry entry[2][INPUT_BUCKETS][2]; // [perspective][bucket][flip?1:0]
    // Cache rows are only valid diff bases for the network they were built
    // with: re-initialise whenever the active network identity changes
    // (EvalFile swap at runtime).
    const Deep::NetworkDeep* builtFor = nullptr;

    inline void ensureInitialised() noexcept {
        if (builtFor == activeNetwork) [[likely]] return;
        const Deep::NetworkDeep& net = *activeNetwork;
        for (auto& perPersp : entry)
            for (auto& perBucket : perPersp)
                for (auto& e : perBucket) {
                    std::memcpy(e.v, net.l0b, sizeof(e.v));
                    std::memset(e.bb, 0, sizeof(e.bb));
                }
        builtFor = activeNetwork;
    }
};

} // namespace NNUE
