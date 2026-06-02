#pragma once

#include <cstdint>
#include <limits>

#include "eval/phase_value.hpp"

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

// Phase-split material values used by the blended evaluator.
// Scalars above remain unchanged (SEE, MVV, search heuristics).
inline int32_t PAWN_VALUE_MG   =   99;
inline int32_t PAWN_VALUE_EG   =  104;
inline int32_t KNIGHT_VALUE_MG =  356;
inline int32_t KNIGHT_VALUE_EG =  371;
inline int32_t BISHOP_VALUE_MG =  384;
inline int32_t BISHOP_VALUE_EG =  396;
inline int32_t ROOK_VALUE_MG   =  511;
inline int32_t ROOK_VALUE_EG   =  567;
inline int32_t QUEEN_VALUE_MG  =  998;
inline int32_t QUEEN_VALUE_EG  = 1007;

// Indexed by piece type (0=EMPTY, 1=PAWN, 2=KNIGHT, 3=BISHOP, 4=ROOK, 5=QUEEN, 6=KING, 7=unused)
inline int32_t PIECE_VALUES[8] = {
    0, PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE, ROOK_VALUE, QUEEN_VALUE, KING_VALUE, 0
};

// ===================================================
// PAWN STRUCTURE EVALUATION
// ===================================================
inline PhaseValue DOUBLED_PAWN_PENALTY      = {-8, -8};
inline PhaseValue ISOLATED_PAWN_PENALTY     = {-17, -17};
inline PhaseValue PASSED_PAWN_BONUS         = {26, 26};
inline PhaseValue PAWN_ISLAND_PENALTY       = {-9, -9};
inline PhaseValue PAWN_SUPPORT_BONUS        = {10, 10};
inline PhaseValue CANDIDATE_PASSER_BONUS    = {10, 14};   // was +4 in EG
inline PhaseValue CONNECTED_PASSER_BONUS    = {22, 28};   // was +6 in EG
inline PhaseValue BACKWARD_PAWN_PENALTY     = {-10, -10};
inline PhaseValue PASSED_PAWN_BLOCKED_PENALTY = {-12, -12};
inline PhaseValue CENTER_CONTROL_BONUS      = {8, 8};

// Passed-pawn rank scaling: previously isEndgame ? 10 : 2 and isEndgame ? 40 : 20.
inline PhaseValue PASSED_ADVANCEMENT_SCALE    = {2, 10};
inline PhaseValue PASSED_NEAR_PROMOTION_BONUS = {20, 40};

// ===================================================
// BISHOP PAIR
// ===================================================
inline PhaseValue BISHOP_PAIR_BONUS         = {32, 32};

// ===================================================
// CASTLING
// ===================================================
inline PhaseValue KING_NON_CASTLING_PENALTY       = {7, 7};
inline PhaseValue KING_LOST_CASTLING_RIGHTS_PENALTY = {17, 17};
inline PhaseValue LOSS_OF_CASTLING_PENALTY        = {22, 22};

// ===================================================
// DEVELOPMENT & INITIATIVE
// ===================================================
inline PhaseValue INIT_BONUS                = {14, 0};   // was MG=14, EG=0
inline PhaseValue DEVELOPMENT_BONUS         = {6, 6};

// ===================================================
// PIECE MOBILITY & TRAPPED PIECES
// ===================================================
inline PhaseValue LOW_MOBILITY_KNIGHT_PENALTY = {8, 8};
inline PhaseValue PINNED_KNIGHT_PENALTY       = {33, 33};
inline PhaseValue LOW_MOBILITY_BISHOP_PENALTY = {11, 11};
inline PhaseValue PINNED_BISHOP_PENALTY       = {37, 37};
inline PhaseValue LOW_MOBILITY_ROOK_PENALTY   = {30, 30};
inline PhaseValue PINNED_ROOK_PENALTY         = {75, 75};
inline PhaseValue LOW_MOBILITY_QUEEN_PENALTY  = {33, 33};
inline PhaseValue PINNED_QUEEN_PENALTY        = {141, 141};
inline PhaseValue MOBILITY_CENTER_BONUS       = {1, 1};
inline PhaseValue MOBILITY_OWN_PAWN_BLOCKER_PENALTY = {4, 4};
inline int32_t QUEEN_EARLY_MOBILITY_THRESHOLD = 8;      // scalar threshold
inline PhaseValue QUEEN_EARLY_MOBILITY_PENALTY = {5, 5};
inline PhaseValue OUTPOST_CENTER_FILE_BONUS    = {4, 4};
inline PhaseValue OUTPOST_NEAR_CENTER_FILE_BONUS = {3, 3};
inline PhaseValue OUTPOST_ADVANCED_RANK_BONUS = {5, 5};
inline PhaseValue OUTPOST_KING_ZONE_BONUS     = {4, 4};
inline PhaseValue OUTPOST_KEY_SQUARE_BONUS    = {1, 1};
inline PhaseValue COORDINATION_PENALTY        = {9, 9};
inline PhaseValue OUTPOST_BISHOP_BONUS        = {11, 11};
inline PhaseValue OUTPOST_KNIGHT_BONUS        = {17, 17};

// ===================================================
// HANGING PIECES
// ===================================================
inline PhaseValue HANGING_PAWN_PENALTY            = {-11, -11};
inline PhaseValue HANGING_PAWN_NEAR_KING_PENALTY  = {-36, -36};
inline PhaseValue HANGING_HOOK_PAWN_PENALTY       = {-15, -15};
inline PhaseValue HANGING_MINOR_PENALTY           = {-25, -25};
inline PhaseValue HANGING_ROOK_PENALTY            = {-35, -35};
inline PhaseValue HANGING_QUEEN_PENALTY           = {-50, -50};

// ===================================================
// EXPLICIT THREATS
// Old code applied a flat 70% scale in endgame. We bake the discount into eg
// values directly so that tuning can re-balance the mg/eg gradient.
// ===================================================
inline PhaseValue THREAT_PAWN_ATTACK_MINOR_PENALTY   = {-30, -21};
inline PhaseValue THREAT_PAWN_ATTACK_ROOK_PENALTY    = {-58, -40};
inline PhaseValue THREAT_PAWN_ATTACK_QUEEN_PENALTY   = {-104, -72};
inline PhaseValue THREAT_MINOR_ATTACK_ROOK_PENALTY   = {-39, -27};
inline PhaseValue THREAT_MINOR_ATTACK_QUEEN_PENALTY  = {-60, -42};
inline PhaseValue THREAT_ROOK_ATTACK_QUEEN_PENALTY   = {-48, -33};
inline PhaseValue THREAT_PAWN_PUSH_MINOR_PENALTY     = {-14, -9};
inline PhaseValue THREAT_PAWN_PUSH_ROOK_PENALTY      = {-28, -19};
inline PhaseValue THREAT_PAWN_PUSH_QUEEN_PENALTY     = {-45, -31};
inline PhaseValue THREAT_LOOSE_MINOR_PENALTY         = {-12, -8};
inline PhaseValue THREAT_LOOSE_ROOK_PENALTY          = {-31, -21};
inline PhaseValue THREAT_LOOSE_QUEEN_PENALTY         = {-42, -29};

// ===================================================
// PAWN FORKS
// ===================================================
inline PhaseValue PAWN_FORK_BASE_BONUS    = {37, 37};
inline PhaseValue PAWN_FORK_MAJOR_BONUS   = {28, 28};
inline PhaseValue PAWN_FORK_ROYAL_BONUS   = {42, 42};

// ===================================================
// BISHOP EVALUATION
// ===================================================
inline int32_t BAD_BISHOP_PAWN_MULTIPLIER = 8;  // scalar multiplier
inline PhaseValue BLOCK_PENALTY_BISHOP    = {34, 34};
inline PhaseValue BLOCK_PENALTY_KNIGHT    = {28, 28};
inline PhaseValue BLOCK_PENALTY_QUEEN     = {24, 24};
inline PhaseValue BLOCK_PENALTY_ROOK      = {18, 18};
inline PhaseValue BLOCK_PENALTY_DEFAULT   = {12, 12};
inline PhaseValue BLOCK_OPENING_BONUS     = {10, 10};
inline PhaseValue BLOCK_MIDGAME_BONUS     = {5, 5};
inline int32_t BLOCK_MIDGAME_THRESHOLD       = 16;
inline int32_t BLOCK_MIDGAME_EARLY_THRESHOLD = 10;
inline PhaseValue BLOCK_PAWN_BISHOP_PENALTY  = {10, 10};
inline PhaseValue BLOCK_PAWN_CENTER_FILE_BONUS = {8, 8};
inline PhaseValue BLOCK_PAWN_START_BONUS     = {6, 6};

// ===================================================
// ROOK EVALUATION
// ===================================================
inline PhaseValue OPEN_FILE_ROOK_BONUS        = {24, 24};
inline PhaseValue SEMI_OPEN_FILE_ROOK_BONUS   = {9, 9};
inline PhaseValue ROOK_ON_SEVENTH_BONUS       = {24, 24};
inline PhaseValue ROOK_BEHIND_OWN_PASSER_BONUS    = {13, 13};
inline PhaseValue ROOK_BEHIND_ENEMY_PASSER_BONUS  = {11, 11};

// Rook endgame: previously only contributed in EG. Encoded as eg-side only.
inline PhaseValue ROOK_EG_EDGE_BONUS      = {0, 26};
inline PhaseValue ROOK_EG_PRESSURE_BONUS  = {0, 17};

// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
inline PhaseValue KING_SAFETY_PENALTY           = {-2, -2};
inline PhaseValue KING_ACTIVITY_BONUS           = {4, 4};
inline PhaseValue CASTLE_PAWN_SUPPORT_BONUS     = {9, 9};
inline PhaseValue KING_SHELTER_STRONG_BONUS     = {9, 9};
inline PhaseValue KING_SHELTER_WEAK_BONUS       = {17, 17};
inline PhaseValue KING_SHELTER_MISSING_PENALTY  = {32, 32};
inline PhaseValue KING_PAWN_STORM_NEAR_PENALTY  = {18, 18};
inline PhaseValue KING_PAWN_STORM_FAR_PENALTY   = {11, 11};
inline PhaseValue KING_CASTLED_SHIELD_BREAK_PENALTY = {10, 10};
inline PhaseValue KING_SHELTER_ADVANCE_ONE_PENALTY  = {3, 3};
inline PhaseValue KING_SHELTER_ADVANCE_TWO_PENALTY  = {4, 4};
inline PhaseValue KING_HOOK_PAWN_ATTACKED_PENALTY   = {13, 13};
inline PhaseValue KING_HOOK_PAWN_HANGING_PENALTY    = {22, 22};
inline PhaseValue KING_SEMI_OPEN_FILE_PENALTY       = {16, 16};
inline PhaseValue KING_OPEN_FILE_PENALTY            = {14, 14};
inline PhaseValue KING_FILE_PRESSURE_PENALTY        = {13, 13};
inline PhaseValue KING_OPEN_DIAGONAL_PENALTY        = {8, 8};
inline int32_t KING_SAFETY_SIDE_CAP                = 168;  // scalar cap
inline int32_t KING_ATTACK_MATERIAL_MIN_SCALE      = 38;
inline int32_t KING_ATTACK_MATERIAL_MAX_SCALE      = 152;
inline int32_t KING_ATTACK_WEIGHT_KNIGHT           = 9;
inline int32_t KING_ATTACK_WEIGHT_BISHOP           = 8;
inline int32_t KING_ATTACK_WEIGHT_ROOK             = 8;
inline int32_t KING_ATTACK_WEIGHT_QUEEN            = 18;
inline int32_t KING_SAFE_CONTACT_BONUS             = 7;
inline int32_t KING_FORCING_CONTACT_BONUS          = 5;
inline int32_t KING_SAFE_CHECK_BONUS               = 12;
inline int32_t KING_FORCING_CHECK_BONUS            = 9;
inline int32_t KING_ATTACK_DANGER_CAP              = 136;

// Scalar tuning levers for king safety (internal multipliers/thresholds).
inline int32_t KING_SHELTER_PAWN_MULTIPLIER        = 12;
inline int32_t KING_ATTACK_QUEEN_WEIGHT            = 34;
inline int32_t KING_ATTACK_ROOK_WEIGHT             = 16;
inline int32_t KING_ATTACK_MINOR_WEIGHT            = 8;
inline int32_t KING_ATTACK_OPEN_FILE_INCREMENT     = 4;
inline int32_t KING_ATTACK_HEAVY_FILE_INCREMENT    = 8;
inline int32_t KING_SHELTER_INIT_DISTANCE          = 99;
inline int32_t KING_SHELTER_VERY_CLOSE             = 1;
inline int32_t KING_SHELTER_CLOSE                  = 2;
inline int32_t KING_SHELTER_FAR                    = 3;
inline int32_t KING_SHELTER_MIN_ADVANCE_CHECK      = 2;
inline int32_t KING_SHELTER_ADVANCE_PAWN_MULTIPLIER = 2;

// ===================================================
// SPACE ADVANTAGE
// ===================================================
inline PhaseValue SPACE_BONUS = {1, 1};

// ===================================================
// STALEMATE HANDLING (scalar — used by search drawn-stalemate logic)
// ===================================================
inline int32_t STALEMATE_DRAW_PENALTY_MAJOR  = 450;
inline int32_t STALEMATE_DRAW_PENALTY_MINOR  = 120;
inline int32_t STALEMATE_MATERIAL_THRESHOLD  = 130;

// ===================================================
// MOVE ORDERING (SEARCH)
// ===================================================
inline int32_t CHECK_BONUS    = 50;
inline int32_t KILLER1_BONUS  = 2000;
inline int32_t KILLER2_BONUS  = 1900;

// ===================================================
// MVV (Most Valuable Victim) table for capture ordering
// ===================================================
inline int32_t MVV_TABLE[7] = {
    0,                  // EMPTY
    PAWN_VALUE * 10,    // PAWN = 1000
    KNIGHT_VALUE * 10,  // KNIGHT = 3200
    BISHOP_VALUE * 10,  // BISHOP = 3300
    ROOK_VALUE * 10,    // ROOK = 5000
    QUEEN_VALUE * 10,   // QUEEN = 9000
    KING_VALUE * 10     // KING = 200000
};

} // namespace engine
