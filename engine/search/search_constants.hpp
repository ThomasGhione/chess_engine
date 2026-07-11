#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

// All tunable search + move-ordering parameters, centralized here the same way
// eval_constants.hpp centralizes evaluation weights. These are compile-time
// constants (not UCI options): the search relies on several of them for array
// dimensions, so they are constexpr rather than mutable inline globals.
//
// Purely structural constants that are tied to a specific local data structure
// (SEE cache size, history flat-array cell counts, tracked-quiet buffer sizes,
// int16 clamp bounds) deliberately stay next to their declarations.

namespace engine {

// ===================================================
// CORE SEARCH SIZING (array dimensions, depth defaults)
// ===================================================
inline constexpr int32_t  MAX_PLY                = 64;
inline constexpr int32_t  CAPTURE_HISTORY_SLOTS  = 2;
inline constexpr int32_t  PAWN_CORR_HISTORY_SIZE = 1 << 14;
inline constexpr int DEFAULT_DEPTH               = 11;

// Scores within MATE_BOUND of ±INF encode a forced mate at a ply distance and
// are rebased on TT store/load (see scoreToTT / scoreFromTT).
inline constexpr int32_t MATE_BOUND = std::numeric_limits<int32_t>::max() - 2048;

// ===================================================
// PRUNING / EXTENSION PARAMETERS
// ===================================================
// The margins below are plain globals (not constexpr) so the SMAC3 campaign
// (#9) can drive them as UCI spins, same pattern as eval_constants.hpp. The
// UCI layer stops any search before writing them. Derived tables
// (FUTILITY_MARGINS, LMP_THRESHOLDS, the LMR table in searcher.cpp) are
// rebuilt by rebuildSearchDerivedTables(); the defaults reproduce the frozen
// June values exactly.
inline constexpr int     NULL_MOVE_VERIFICATION_DEPTH = 10;
// Null-move reduction eval scaling: deeper reduction the further eval beats beta.
inline int32_t NMP_EVAL_DIV = 150;
inline int32_t NMP_EVAL_MAX = 4;
// Reverse futility pruning margin per remaining ply.
inline int32_t RFP_MARGIN_PER_DEPTH = 90;
// Futility margin generators: mid row = MID_STEP*d, endgame row =
// EG_BASE + EG_STEP*(d-1). FUTILITY_MARGINS[isLateEndgame][depth] is the
// derived table consumed by the move loop (gated to depth 1..6).
inline int32_t FUTILITY_MID_STEP = 260;
inline int32_t FUTILITY_EG_BASE  = 170;
inline int32_t FUTILITY_EG_STEP  = 180;
inline int32_t FUTILITY_MARGINS[2][7] = {
    {0, 260, 520, 780, 1040, 1300, 1560},
    {0, 170, 350, 530,  710,  890, 1070},
};
// LMP_THRESHOLDS[improving][isLateEndgame][depth]: higher = more permissive.
// Gated to depth 1..4. Derived: base table scaled by LMP_SCALE_PCT[improving].
inline constexpr int LMP_BASE_THRESHOLDS[2][2][5] = {
    {{0, 12, 20, 30, 42}, {0, 16, 26, 38, 52}},
    {{0, 16, 26, 38, 52}, {0, 20, 32, 46, 62}},
};
inline int32_t LMP_SCALE_PCT[2] = {100, 100};
inline int LMP_THRESHOLDS[2][2][5] = {
    {{0, 12, 20, 30, 42}, {0, 16, 26, 38, 52}},
    {{0, 16, 26, 38, 52}, {0, 20, 32, 46, 62}},
};
// History-based quiet pruning: skip quiet moves with very negative history.
// Indexed by depth (0..3); depth 0 unused.
inline int32_t HISTORY_PRUNE_THRESHOLD[4] = {0, -4096, -6144, -8192};

// SEE capture pruning: skip captures with SEE < -SEE_CAPTURE_MARGIN * depth.
inline int32_t SEE_CAPTURE_MARGIN = 70;
// Singular extension.
inline constexpr int SE_MIN_DEPTH     = 6;
inline constexpr int SE_DEPTH_MARGIN  = 3;
inline int32_t SE_BETA_MARGIN   = 3;  // seBeta = ttScore - margin*depth
inline int32_t SE_DOUBLE_MARGIN = 16; // double-extend when seScore < seBeta - 16
// ProbCut.
inline int32_t PROBCUT_MARGIN    = 80;
inline constexpr int32_t PROBCUT_MIN_DEPTH = 3;

// Rebuilds the derived tables above plus the LMR reduction table
// (searcher.cpp) from the current generator values. Called by the UCI layer
// after writing a generator option; never call during a live search.
void rebuildSearchDerivedTables() noexcept;

// ===================================================
// LMR (late move reductions) table parameters
// ===================================================
// LMR_C == LMR_C_PERCENT / 100.0 (int spin for the tuner).
inline int32_t LMR_C_PERCENT = 300;
inline constexpr int    LMR_MAX_DEPTH = 20;  // engine never exceeds depth 14 in practice
inline constexpr int    LMR_MAX_MOVES = 218; // theoretical maximum legal moves (== MAX_MOVES, movelist.hpp)

// ===================================================
// HISTORY HEURISTIC BOUNDS
// ===================================================
inline constexpr int32_t MAX_HISTORY         = 16384;
inline constexpr int32_t MAX_CAPTURE_HISTORY = 10000;

// ===================================================
// CORRECTION HISTORY (search - static eval residual)
// ===================================================
inline constexpr int32_t CORR_HIST_LIMIT   = 1024; // bound on the smoothed residual (cp)
inline constexpr int32_t CORR_HIST_DIVISOR = 4;    // applied fraction of each residual
inline constexpr int32_t CORR_HIST_BLEND   = 256;  // weighted-average denominator
inline constexpr int32_t CORR_HIST_MAX_W   = 16;   // per-update weight cap (grows with depth)
// Cap on the SUM of the pawn/minor/major corrections, kept at the old pawn-only 256 cp.
inline constexpr int32_t CORR_TOTAL_CAP    = CORR_HIST_LIMIT / CORR_HIST_DIVISOR;

// ===================================================
// QUIESCENCE SEARCH
// ===================================================
inline constexpr uint8_t MAX_QSEARCH_DEPTH = 48;
inline constexpr int32_t QSEARCH_PAWN_PROMO_DELTA          = 150;
inline constexpr int32_t QSEARCH_MATERIAL_BAD              = -400;
inline constexpr int32_t QSEARCH_MATERIAL_WORSE            = -200;
inline constexpr int32_t QSEARCH_MATERIAL_BAD_DELTA        = 150;
inline constexpr int32_t QSEARCH_MATERIAL_WORSE_DELTA      = 75;
inline constexpr int32_t QSEARCH_DEPTH_REDUCTION_THRESHOLD = 5;
inline constexpr int32_t QSEARCH_DEPTH_REDUCTION_PER_5     = 50;
inline constexpr int32_t QSEARCH_DELTAMARGIN_MIN           = 960; // == QUEEN_VALUE
// Near-promotion pawn masks (7th rank for each side).
inline constexpr uint64_t WHITE_NEAR_PROMO_PAWNS = 0x00FF000000000000ULL;
inline constexpr uint64_t BLACK_NEAR_PROMO_PAWNS = 0x000000000000FF00ULL;

// ===================================================
// ASPIRATION WINDOW
// ===================================================
inline constexpr int32_t WINDOW_HARD_CAP    = 1500;
inline constexpr int     MAX_ASP_RESEARCHES = 6;

// ===================================================
// DRAW / CONTEMPT SCORING
// ===================================================
inline constexpr int32_t REPETITION_CONTEMPT = 80; // ~0.8 pawn

// ===================================================
// PARALLELISM (Lazy SMP)
// ===================================================
// Hard cap on helper threads (Threads option is already clamped to hwmax).
inline constexpr int MAX_HELPER_THREADS = 63;

// ===================================================
// CONTINUATION HISTORY layout
// ===================================================
// contHist is keyed by the previous move's (side, pieceType, toSq); each context
// holds a [PIECE_TYPES][64] PieceTo block indexed by the CURRENT move's
// (pieceType, toSq). Piece types are 0..6 (EMPTY..KING), so the block is 7*64.
inline constexpr int CONT_HIST_PIECE_TYPES   = 7;
inline constexpr int CONT_HIST_PIECE_STRIDE  = 64;
inline constexpr int contHistIndex(int pieceType, int toSq) noexcept {
    return pieceType * CONT_HIST_PIECE_STRIDE + toSq;
}

// ===================================================
// MOVE ORDERING (sorter) — score buckets
// ===================================================
inline constexpr int32_t HASH_MOVE_SCORE      = 100000;
inline constexpr int32_t CAPTURE_BASE_SCORE   = 10000;
inline constexpr int32_t KILLER_1_SCORE       = 9000;
inline constexpr int32_t KILLER_2_SCORE       = 8500;
inline constexpr int32_t COUNTER_MOVE_SCORE   = 8200;
inline constexpr int32_t PROMOTION_BASE_SCORE = 7000;
inline constexpr int32_t HISTORY_SCORE_MAX    = 7500;
inline constexpr int32_t HISTORY_SCORE_MIN    = -2000;
// Qsearch tactical move scoring.
inline constexpr int32_t TACTICAL_PROMOTION_SCORE = 9000;
inline constexpr int32_t FUTILITY_MARGIN          = 100;
inline constexpr int32_t MOVE_DELTA_MARGIN        = 140;
inline constexpr int32_t SEE_THRESHOLD_SHALLOW    = -24; // ply < 10
inline constexpr int32_t SEE_THRESHOLD_MID        = -12; // 10 <= ply < 20
inline constexpr int32_t SEE_THRESHOLD_DEEP       = -4;  // ply >= 20

} // namespace engine
