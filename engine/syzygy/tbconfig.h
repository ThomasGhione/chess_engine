/*
 * tbconfig.h — Pyrrhic configuration for HydraY.
 *
 * Pyrrhic square convention: a1=0 (LSB) .. h8=63 (MSB), rank-minor.
 * All attack functions use this convention internally.
 *
 * Sliding attacks use the Hyperbola Quintessence formula, which is
 * branchless and requires no lookup tables beyond the line masks.
 */

#pragma once

#include <stdint.h>

/* ── bit primitives ──────────────────────────────────────────────────────── */

#define PYRRHIC_POPCOUNT(x)         (__builtin_popcountll(x))
#define PYRRHIC_LSB(x)              (__builtin_ctzll(x))

/* PYRRHIC_POPLSB(p): clear the LSB of *p and return its bit INDEX (0-63).
 * tbprobe.c::fill_squares uses the return value as a square index into
 * tables like Triangle[64], so returning the post-clear bitboard value
 * (as a plain expression macro would do) causes out-of-bounds crashes. */
static inline unsigned pyrrhic_poplsb(uint64_t *b) {
    unsigned s = (unsigned)__builtin_ctzll(*b);
    *b &= *b - 1;
    return s;
}
#define PYRRHIC_POPLSB(p)           pyrrhic_poplsb(p)

/* ── helper: Hyperbola Quintessence slider attacks ───────────────────────── */

/* Full 64-bit bit reversal: byte swap + per-byte bit swap. Required by the
 * HQ "negative direction" trick on lines that live inside a single byte
 * (ranks): bswap64 alone only reorders bytes, so for rank attacks the
 * slider stays at the same intra-byte position and the subtraction trick
 * computes the wrong ray. True bit reversal mirrors the line correctly. */
static inline uint64_t tb_reverse64(uint64_t x) {
    x = __builtin_bswap64(x);
    x = ((x & UINT64_C(0xF0F0F0F0F0F0F0F0)) >> 4) | ((x & UINT64_C(0x0F0F0F0F0F0F0F0F)) << 4);
    x = ((x & UINT64_C(0xCCCCCCCCCCCCCCCC)) >> 2) | ((x & UINT64_C(0x3333333333333333)) << 2);
    x = ((x & UINT64_C(0xAAAAAAAAAAAAAAAA)) >> 1) | ((x & UINT64_C(0x5555555555555555)) << 1);
    return x;
}

static inline uint64_t tb_hq(uint64_t occ, uint64_t mask, uint64_t slider) {
    const uint64_t o = occ & mask;
    const uint64_t positive = (o - 2 * slider) ^ o;
    const uint64_t o_rev = tb_reverse64(o);
    const uint64_t s_rev = tb_reverse64(slider);
    const uint64_t negative = tb_reverse64((o_rev - 2 * s_rev) ^ o_rev);
    return (positive | negative) & mask;
}

/* ── file / rank / diagonal masks (computed once inline) ─────────────────── */

static inline uint64_t tb_file_mask(int sq) {
    return UINT64_C(0x0101010101010101) << (sq & 7);
}
static inline uint64_t tb_rank_mask(int sq) {
    return UINT64_C(0xFF) << (sq & ~7);
}
static inline uint64_t tb_diag_mask(int sq) {
    /* main diagonal (a1-h8 direction) */
    static const uint64_t d[15] = {
        0x0100000000000000ULL, 0x0201000000000000ULL, 0x0402010000000000ULL,
        0x0804020100000000ULL, 0x1008040201000000ULL, 0x2010080402010000ULL,
        0x4020100804020100ULL, 0x8040201008040201ULL, 0x0080402010080402ULL,
        0x0000804020100804ULL, 0x0000008040201008ULL, 0x0000000080402010ULL,
        0x0000000000804020ULL, 0x0000000000008040ULL, 0x0000000000000080ULL,
    };
    /* d[i] is the diagonal with rank-file == 7-i (verified by inspection of
     * the table above). For a square with rank-file == k we want index 7-k,
     * i.e. file - rank + 7. */
    return d[(sq & 7) - (sq >> 3) + 7];
}
static inline uint64_t tb_anti_mask(int sq) {
    /* anti-diagonal (a8-h1 direction) */
    static const uint64_t d[15] = {
        0x0000000000000001ULL, 0x0000000000000102ULL, 0x0000000000010204ULL,
        0x0000000001020408ULL, 0x0000000102040810ULL, 0x0000010204081020ULL,
        0x0001020408102040ULL, 0x0102040810204080ULL, 0x0204081020408000ULL,
        0x0408102040800000ULL, 0x0810204080000000ULL, 0x1020408000000000ULL,
        0x2040800000000000ULL, 0x4080000000000000ULL, 0x8000000000000000ULL,
    };
    return d[(sq >> 3) + (sq & 7)];
}

/* ── sliding piece attacks ───────────────────────────────────────────────── */

#define PYRRHIC_ROOK_ATTACKS(sq, occ) \
    (tb_hq((occ), tb_file_mask(sq), UINT64_C(1) << (sq)) | \
     tb_hq((occ), tb_rank_mask(sq), UINT64_C(1) << (sq)))

#define PYRRHIC_BISHOP_ATTACKS(sq, occ) \
    (tb_hq((occ), tb_diag_mask(sq), UINT64_C(1) << (sq)) | \
     tb_hq((occ), tb_anti_mask(sq), UINT64_C(1) << (sq)))

#define PYRRHIC_QUEEN_ATTACKS(sq, occ) \
    (PYRRHIC_ROOK_ATTACKS((sq), (occ)) | PYRRHIC_BISHOP_ATTACKS((sq), (occ)))

/* ── leaper piece attacks ────────────────────────────────────────────────── */

static inline uint64_t tb_knight_attacks(int sq) {
    const uint64_t b = UINT64_C(1) << sq;
    return (((b << 17) | (b >> 15)) & UINT64_C(0xFEFEFEFEFEFEFEFE))
         | (((b << 15) | (b >> 17)) & UINT64_C(0x7F7F7F7F7F7F7F7F))
         | (((b << 10) | (b >>  6)) & UINT64_C(0xFCFCFCFCFCFCFCFC))
         | (((b <<  6) | (b >> 10)) & UINT64_C(0x3F3F3F3F3F3F3F3F));
}

static inline uint64_t tb_king_attacks(int sq) {
    const uint64_t b = UINT64_C(1) << sq;
    const uint64_t lr = ((b >> 1) & UINT64_C(0x7F7F7F7F7F7F7F7F))
                      | ((b << 1) & UINT64_C(0xFEFEFEFEFEFEFEFE));
    return (lr | b) << 8 | (lr | b) >> 8 | lr;
}

/* Pawn attacks: c=1 White (moves up = +8), c=0 Black (moves down = -8).
 * sq is the square of the attacking pawn; returns the set of squares attacked. */
static inline uint64_t tb_pawn_attacks(int sq, int c) {
    const uint64_t b = UINT64_C(1) << sq;
    if (c) /* White */ return ((b << 9) & UINT64_C(0xFEFEFEFEFEFEFEFE))
                            | ((b << 7) & UINT64_C(0x7F7F7F7F7F7F7F7F));
    else   /* Black */ return ((b >> 7) & UINT64_C(0xFEFEFEFEFEFEFEFE))
                            | ((b >> 9) & UINT64_C(0x7F7F7F7F7F7F7F7F));
}

#define PYRRHIC_KNIGHT_ATTACKS(sq)       tb_knight_attacks(sq)
#define PYRRHIC_KING_ATTACKS(sq)         tb_king_attacks(sq)
#define PYRRHIC_PAWN_ATTACKS(sq, c)      tb_pawn_attacks((sq), (c))
