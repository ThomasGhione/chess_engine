#pragma once

#include <cstdint>
#include <limits>

// Piece values and search-scoring constants. This is what survives of the
// handcrafted evaluator's constant set (removed in the NNUE era): these
// values feed SEE, MVV capture ordering, the incremental material delta used
// for stalemate scoring, and a few search thresholds — not evaluation.

namespace engine {

// ===================================================
// PIECE BASE VALUES (scalar — used for SEE, MVV, material delta)
// ===================================================
inline int32_t PAWN_VALUE   =       100;
inline int32_t KNIGHT_VALUE =       344;
inline int32_t BISHOP_VALUE =       359;
inline int32_t ROOK_VALUE   =       502;
inline int32_t QUEEN_VALUE  =       960;
inline int32_t KING_VALUE   =    20'000;
inline int32_t MATE_SCORE   = std::numeric_limits<int32_t>::max();

// Indexed by piece type (0=EMPTY, 1=PAWN, 2=KNIGHT, 3=BISHOP, 4=ROOK, 5=QUEEN, 6=KING, 7=unused)
inline int32_t PIECE_VALUES[8] = {
    0, PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE, ROOK_VALUE, QUEEN_VALUE, KING_VALUE, 0
};

// ===================================================
// STALEMATE SCORING (search: stalemateScoreFromMaterialDelta)
// ===================================================
inline int32_t STALEMATE_DRAW_PENALTY_MAJOR  = 450;
inline int32_t STALEMATE_DRAW_PENALTY_MINOR  = 120;
inline int32_t STALEMATE_MATERIAL_THRESHOLD  = 130;

// ===================================================
// MVV (Most Valuable Victim) table for capture ordering
// ===================================================
inline int32_t MVV_TABLE[7] = {
    0,                  // EMPTY
    PAWN_VALUE * 10,    // PAWN
    KNIGHT_VALUE * 10,  // KNIGHT
    BISHOP_VALUE * 10,  // BISHOP
    ROOK_VALUE * 10,    // ROOK
    QUEEN_VALUE * 10,   // QUEEN
    KING_VALUE * 10     // KING
};

} // namespace engine
