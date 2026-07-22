#pragma once

#include <cstdint>
#include <array>

// =============================================================================
// SLIDING-PIECE RELEVANT-OCCUPANCY MASKS
// =============================================================================
// The rook/bishop "relevant occupancy" masks that feed PEXT (and, on a non-BMI2
// fallback, the magic multiply) plus the compile-time attack-table build.
//
// These used to be a hand-pasted table of 128 magic multipliers + 128 masks.
// The magics are gone (the engine indexes with _pext_u64 — see piece.hpp), and
// the masks are now GENERATED at compile time here instead of stored as
// literals: rook = its rank+file minus the board edges, bishop = its four
// diagonals minus the edges (edge squares never block, so they carry no index
// bit). The generators reproduced the old hand table byte-for-byte across all
// 64 squares; the static_asserts below pin a few known values so a wrong edit
// is caught at compile time (nnue-selftest + perft cover the rest).
// =============================================================================

namespace pieces {

// Rook: same-file ranks and same-rank files, excluding the square itself and
// the four edges (files A/H, ranks 1/8).
constexpr uint64_t rookRelevantMask(int sq) noexcept {
    const int f = sq & 7;
    const int r = sq >> 3;
    uint64_t m = 0ULL;
    for (int rr = 1; rr <= 6; ++rr) if (rr != r) m |= 1ULL << (rr * 8 + f);
    for (int ff = 1; ff <= 6; ++ff) if (ff != f) m |= 1ULL << (r * 8 + ff);
    return m;
}

// Bishop: the four diagonals, walked until they reach an edge rank/file (1..6).
constexpr uint64_t bishopRelevantMask(int sq) noexcept {
    const int f = sq & 7;
    const int r = sq >> 3;
    uint64_t m = 0ULL;
    for (int rr = r + 1, ff = f + 1; rr <= 6 && ff <= 6; ++rr, ++ff) m |= 1ULL << (rr * 8 + ff);
    for (int rr = r + 1, ff = f - 1; rr <= 6 && ff >= 1; ++rr, --ff) m |= 1ULL << (rr * 8 + ff);
    for (int rr = r - 1, ff = f + 1; rr >= 1 && ff <= 6; --rr, ++ff) m |= 1ULL << (rr * 8 + ff);
    for (int rr = r - 1, ff = f - 1; rr >= 1 && ff >= 1; --rr, --ff) m |= 1ULL << (rr * 8 + ff);
    return m;
}

inline constexpr std::array<uint64_t, 64> ROOK_MASKS = [] {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) masks[sq] = rookRelevantMask(sq);
    return masks;
}();

inline constexpr std::array<uint64_t, 64> BISHOP_MASKS = [] {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) masks[sq] = bishopRelevantMask(sq);
    return masks;
}();

// Byte-for-byte anchors against the retired hand table (corners, centre, edge).
static_assert(ROOK_MASKS[0]    == 0x000101010101017EULL, "rook A8 mask");
static_assert(ROOK_MASKS[4]    == 0x001010101010106EULL, "rook E8 mask");
static_assert(ROOK_MASKS[36]   == 0x0010106E10101000ULL, "rook E4 mask");
static_assert(ROOK_MASKS[63]   == 0x7E80808080808000ULL, "rook H1 mask");
static_assert(BISHOP_MASKS[0]  == 0x0040201008040200ULL, "bishop A8 mask");
static_assert(BISHOP_MASKS[27] == 0x0040221400142200ULL, "bishop D5 mask");
static_assert(BISHOP_MASKS[63] == 0x0040201008040200ULL, "bishop H1 mask");

} // namespace pieces
