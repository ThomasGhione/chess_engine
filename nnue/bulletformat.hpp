#pragma once

// bulletformat writer/decoder for NNUE training data (NNUE_PLAN.md, Fase 0/1).
//
// BulletRecord mirrors bulletformat::ChessBoard byte-for-byte (32 bytes); the
// packing below is a literal transcription of ChessBoard::from_raw from
// https://github.com/jw1912/bulletformat/blob/main/src/chess.rs
//
// Stored positions are normalized to the side to move ("stm frame"): when
// black is to move every bitboard is vertically mirrored, colors are swapped
// and score/result flip sign, so the stored "white" is always the stm.
//   pcs    : one nibble per occupied square in ascending LERF order (a1=0),
//            low nibble first; nibble = (isOpponent << 3) | piece (0=P..5=K)
//   score  : stm-relative centipawns
//   result : 0 = stm loss, 1 = draw, 2 = stm win
//   ksq    : stm king square; oppKsq: opponent king square ^ 56 (its own view)
// Castling/en-passant/clocks are not part of the format.

#include <bit>
#include <cstdint>
#include <string>
#include <type_traits>

#include "../board/board.hpp"

namespace NNUE {

struct BulletRecord {
    uint64_t occ = 0;
    uint8_t  pcs[16] = {};
    int16_t  score = 0;
    uint8_t  result = 0;
    uint8_t  ksq = 0;
    uint8_t  oppKsq = 0;
    uint8_t  extra[3] = {};
};

static_assert(sizeof(BulletRecord) == 32, "must match bulletformat::ChessBoard");
static_assert(std::is_trivially_copyable_v<BulletRecord>);

// A packed position waiting for the game outcome: bulletformat stores the
// result stm-relative, so the stm at record time must survive until game end.
struct PendingRecord {
    BulletRecord record;
    bool blackToMove = false;
};

// Engine bitboards put bit i on engine square i (0 = a8, 63 = h1); bulletformat
// wants LERF (0 = a1). The square map is i ^ 56, which on a whole bitboard is a
// byteswap (vertical mirror).
[[nodiscard]] inline uint64_t toLerf(uint64_t engineBB) noexcept {
    return std::byteswap(engineBB);
}

// whiteScoreCp is white-relative; result is filled in later by finalizeResult.
[[nodiscard]] inline PendingRecord packPosition(const chess::Board& b,
                                                int16_t whiteScoreCp) noexcept {
    using chess::Board;

    // from_raw contract: bitboards in order White, Black, P, N, B, R, Q, K.
    uint64_t bbs[8];
    bbs[0] = toLerf(b.pawns_bb[0] | b.knights_bb[0] | b.bishops_bb[0]
                  | b.rooks_bb[0] | b.queens_bb[0]  | b.kings_bb[0]);
    bbs[1] = toLerf(b.pawns_bb[1] | b.knights_bb[1] | b.bishops_bb[1]
                  | b.rooks_bb[1] | b.queens_bb[1]  | b.kings_bb[1]);
    bbs[2] = toLerf(b.pawns_bb[0]   | b.pawns_bb[1]);
    bbs[3] = toLerf(b.knights_bb[0] | b.knights_bb[1]);
    bbs[4] = toLerf(b.bishops_bb[0] | b.bishops_bb[1]);
    bbs[5] = toLerf(b.rooks_bb[0]   | b.rooks_bb[1]);
    bbs[6] = toLerf(b.queens_bb[0]  | b.queens_bb[1]);
    bbs[7] = toLerf(b.kings_bb[0]   | b.kings_bb[1]);

    int16_t score = whiteScoreCp;
    const bool blackToMove = (b.getActiveColor() == Board::BLACK);
    if (blackToMove) {
        for (uint64_t& bb : bbs) bb = std::byteswap(bb);
        std::swap(bbs[0], bbs[1]);
        score = static_cast<int16_t>(-score);
    }

    PendingRecord out;
    out.blackToMove = blackToMove;
    BulletRecord& r = out.record;
    r.occ = bbs[0] | bbs[1];
    r.score = score;

    uint64_t occ = r.occ;
    int idx = 0;
    while (occ != 0) {
        const uint64_t bit = occ & (0 - occ);
        occ &= occ - 1;
        const uint8_t colour = ((bit & bbs[1]) != 0) ? 8 : 0;
        uint8_t piece = 0;
        for (int p = 2; p < 8; ++p) {
            if ((bbs[p] & bit) != 0) { piece = static_cast<uint8_t>(p - 2); break; }
        }
        r.pcs[idx / 2] |= static_cast<uint8_t>((colour | piece) << (4 * (idx & 1)));
        ++idx;
    }

    r.ksq    = static_cast<uint8_t>(std::countr_zero(bbs[0] & bbs[7]));
    r.oppKsq = static_cast<uint8_t>(std::countr_zero(bbs[1] & bbs[7]) ^ 56);
    return out;
}

// whiteResultX2: 0 = black win, 1 = draw, 2 = white win.
inline void finalizeResult(PendingRecord& p, int whiteResultX2) noexcept {
    p.record.result = static_cast<uint8_t>(
        p.blackToMove ? 2 - whiteResultX2 : whiteResultX2);
}

// FEN of the stored (stm-frame) position, for eyeball checks in datagen-dump.
// The stm is printed as White; castling/ep are not stored, so "w - -".
[[nodiscard]] inline std::string recordToFen(const BulletRecord& r) {
    static constexpr char PIECE_CHARS[16] = {
        'P', 'N', 'B', 'R', 'Q', 'K', '?', '?',
        'p', 'n', 'b', 'r', 'q', 'k', '?', '?'
    };

    char squares[64];
    for (char& c : squares) c = '\0';
    uint64_t occ = r.occ;
    int idx = 0;
    while (occ != 0) {
        const int sq = std::countr_zero(occ);
        occ &= occ - 1;
        const uint8_t nibble = (r.pcs[idx / 2] >> (4 * (idx & 1))) & 0xF;
        squares[sq] = PIECE_CHARS[nibble];
        ++idx;
    }

    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int emptyRun = 0;
        for (int file = 0; file < 8; ++file) {
            const char c = squares[rank * 8 + file];
            if (c == '\0') { ++emptyRun; continue; }
            if (emptyRun > 0) { fen += static_cast<char>('0' + emptyRun); emptyRun = 0; }
            fen += c;
        }
        if (emptyRun > 0) fen += static_cast<char>('0' + emptyRun);
        if (rank > 0) fen += '/';
    }
    fen += " w - - 0 1";
    return fen;
}

} // namespace NNUE
