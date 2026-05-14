#pragma once

#include <cstdint>
#include <limits>

namespace engine {

// ===================================================
// PIECE BASE VALUES
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
// GAME PHASE
// ===================================================
inline int32_t PHASE_FINAL_THRESHOLD = 8;

// ===================================================
// PAWN STRUCTURE EVALUATION
// ===================================================
inline int32_t DOUBLED_PAWN_PENALTY = -9;
inline int32_t ISOLATED_PAWN_PENALTY = -13;
inline int32_t PASSED_PAWN_BONUS = 20;
inline int32_t PAWN_ISLAND_PENALTY = -12;
inline int32_t PAWN_SUPPORT_BONUS = 10;
inline int32_t CANDIDATE_PASSER_BONUS = 10;
inline int32_t CONNECTED_PASSER_BONUS = 23;
inline int32_t BACKWARD_PAWN_PENALTY = -12;
inline int32_t PASSED_PAWN_BLOCKED_PENALTY = -21;
inline int32_t CENTER_CONTROL_BONUS = 8;

// ===================================================
// BISHOP PAIR
// ===================================================
inline int32_t BISHOP_PAIR_BONUS = 32;

// ===================================================
// CASTLING
// ===================================================
inline int32_t KING_NON_CASTLING_PENALTY = 7;
inline int32_t KING_LOST_CASTLING_RIGHTS_PENALTY = 17;
inline int32_t LOSS_OF_CASTLING_PENALTY = 22;

// ===================================================
// DEVELOPMENT & INITIATIVE
// ===================================================
inline int32_t INIT_BONUS_MG = 3;
inline int32_t INIT_BONUS_EG = 0;
inline int32_t DEVELOPMENT_BONUS = 6;

// ===================================================
// PIECE MOBILITY & TRAPPED PIECES
// ===================================================
inline int32_t LOW_MOBILITY_KNIGHT_PENALTY = 8;
inline int32_t PINNED_KNIGHT_PENALTY = 33;
inline int32_t LOW_MOBILITY_BISHOP_PENALTY = 11;
inline int32_t PINNED_BISHOP_PENALTY = 37;
inline int32_t LOW_MOBILITY_ROOK_PENALTY = 30;
inline int32_t PINNED_ROOK_PENALTY = 75;
inline int32_t LOW_MOBILITY_QUEEN_PENALTY = 33;
inline int32_t PINNED_QUEEN_PENALTY = 141;
inline int32_t MOBILITY_CENTER_BONUS = 1;
inline int32_t MOBILITY_OWN_PAWN_BLOCKER_PENALTY = 4;
inline int32_t QUEEN_EARLY_MOBILITY_THRESHOLD = 8;
inline int32_t QUEEN_EARLY_MOBILITY_PENALTY = 5;
inline int32_t OUTPOST_CENTER_FILE_BONUS = 4;
inline int32_t OUTPOST_NEAR_CENTER_FILE_BONUS = 3;
inline int32_t OUTPOST_ADVANCED_RANK_BONUS = 5;
inline int32_t OUTPOST_KING_ZONE_BONUS = 4;
inline int32_t OUTPOST_KEY_SQUARE_BONUS = 1;
inline int32_t COORDINATION_PENALTY = 9;
inline int32_t OUTPOST_BISHOP_BONUS = 11;
inline int32_t OUTPOST_KNIGHT_BONUS = 17;
inline int32_t ORDERING_PENALTY_SAME_PAWN_OPENING = -15;

// ===================================================
// HANGING PIECES
// ===================================================
inline int32_t HANGING_PAWN_PENALTY = -12;
inline int32_t HANGING_PAWN_NEAR_KING_PENALTY = -28;
inline int32_t HANGING_HOOK_PAWN_PENALTY = -16;
inline int32_t HANGING_MINOR_PENALTY = -25;
inline int32_t HANGING_ROOK_PENALTY = -35;
inline int32_t HANGING_QUEEN_PENALTY = -50;

// ===================================================
// EXPLICIT THREATS
// ===================================================
inline int32_t THREAT_PAWN_ATTACK_MINOR_PENALTY = -29;
inline int32_t THREAT_PAWN_ATTACK_ROOK_PENALTY = -58;
inline int32_t THREAT_PAWN_ATTACK_QUEEN_PENALTY = -104;
inline int32_t THREAT_MINOR_ATTACK_ROOK_PENALTY = -40;
inline int32_t THREAT_MINOR_ATTACK_QUEEN_PENALTY = -58;
inline int32_t THREAT_ROOK_ATTACK_QUEEN_PENALTY = -49;
inline int32_t THREAT_PAWN_PUSH_MINOR_PENALTY = -14;
inline int32_t THREAT_PAWN_PUSH_ROOK_PENALTY = -28;
inline int32_t THREAT_PAWN_PUSH_QUEEN_PENALTY = -45;
inline int32_t THREAT_LOOSE_MINOR_PENALTY = -11;
inline int32_t THREAT_LOOSE_ROOK_PENALTY = -32;
inline int32_t THREAT_LOOSE_QUEEN_PENALTY = -42;

// Magic number constant for threat scaling (tunable)
inline int32_t ENDGAME_THREAT_SCALE_PERCENT = 70;              // Reduce threats value in endgame

// ===================================================
// PAWN FORKS (defended pawn attacking 2+ enemy pieces)
// ===================================================
inline int32_t PAWN_FORK_BASE_BONUS       = 37;  // Base bonus for any defended pawn fork
inline int32_t PAWN_FORK_MAJOR_BONUS      = 28;  // Extra if forking at least one rook/queen
inline int32_t PAWN_FORK_ROYAL_BONUS      = 42;  // Extra if forking the king (fork + check)

// ===================================================
// BISHOP EVALUATION (tunable)
// ===================================================
inline int32_t BAD_BISHOP_PAWN_MULTIPLIER = 8;        // Bad bishop pawn penalty factor
inline int32_t BLOCK_PENALTY_BISHOP = 34;             // Central block penalty: bishop
inline int32_t BLOCK_PENALTY_KNIGHT = 28;             // Central block penalty: knight
inline int32_t BLOCK_PENALTY_QUEEN = 24;              // Central block penalty: queen
inline int32_t BLOCK_PENALTY_ROOK = 18;               // Central block penalty: rook
inline int32_t BLOCK_PENALTY_DEFAULT = 12;            // Central block penalty: default
inline int32_t BLOCK_OPENING_BONUS = 10;              // Early opening bonus (moves <= 10)
inline int32_t BLOCK_MIDGAME_BONUS = 5;               // Midgame bonus (moves <= 16)
inline int32_t BLOCK_MIDGAME_THRESHOLD = 16;          // Move threshold for midgame bonus
inline int32_t BLOCK_MIDGAME_EARLY_THRESHOLD = 10;    // Move threshold for opening bonus
inline int32_t BLOCK_PAWN_BISHOP_PENALTY = 10;        // Bishop blocking pawn base penalty
inline int32_t BLOCK_PAWN_CENTER_FILE_BONUS = 8;      // Extra penalty on center files (d,e)
inline int32_t BLOCK_PAWN_START_BONUS = 6;            // Extra penalty if pawn on starting rank
inline int32_t OPEN_FILE_ROOK_BONUS = 24;
inline int32_t SEMI_OPEN_FILE_ROOK_BONUS = 9;
inline int32_t ROOK_ON_SEVENTH_BONUS = 24;
inline int32_t ROOK_BEHIND_OWN_PASSER_BONUS = 13;
inline int32_t ROOK_BEHIND_ENEMY_PASSER_BONUS = 11;

// ===================================================
// ROOK ENDGAME (R+K vs K)
// ===================================================
inline int32_t ROOK_EG_EDGE_BONUS = 26;
inline int32_t ROOK_EG_PRESSURE_BONUS = 17;


// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
inline int32_t KING_SAFETY_PENALTY = -2;
inline int32_t KING_ACTIVITY_BONUS = 4;
inline int32_t CASTLE_PAWN_SUPPORT_BONUS = 7;
inline int32_t KING_SHELTER_STRONG_BONUS = 13;
inline int32_t KING_SHELTER_WEAK_BONUS = 17;
inline int32_t KING_SHELTER_MISSING_PENALTY = 17;
inline int32_t KING_PAWN_STORM_NEAR_PENALTY = 18;
inline int32_t KING_PAWN_STORM_FAR_PENALTY = 14;
inline int32_t KING_CASTLED_SHIELD_BREAK_PENALTY = 12;
inline int32_t KING_SHELTER_ADVANCE_ONE_PENALTY = 3;
inline int32_t KING_SHELTER_ADVANCE_TWO_PENALTY = 4;
inline int32_t KING_HOOK_PAWN_ATTACKED_PENALTY = 19;
inline int32_t KING_HOOK_PAWN_HANGING_PENALTY = 25;
inline int32_t KING_SAFETY_OPENING_SCALE_PERCENT = 33;
inline int32_t KING_SEMI_OPEN_FILE_PENALTY = 17;
inline int32_t KING_OPEN_FILE_PENALTY = 10;
inline int32_t KING_FILE_PRESSURE_PENALTY = 8;
inline int32_t KING_OPEN_DIAGONAL_PENALTY = 10;
inline int32_t KING_SAFETY_SIDE_CAP = 168;
inline int32_t KING_ATTACK_MATERIAL_MIN_SCALE = 38;
inline int32_t KING_ATTACK_MATERIAL_MAX_SCALE = 139;
inline int32_t KING_ATTACK_WEIGHT_KNIGHT = 7;
inline int32_t KING_ATTACK_WEIGHT_BISHOP = 6;
inline int32_t KING_ATTACK_WEIGHT_ROOK   = 12;
inline int32_t KING_ATTACK_WEIGHT_QUEEN  = 22;
inline int32_t KING_SAFE_CONTACT_BONUS = 5;
inline int32_t KING_FORCING_CONTACT_BONUS = 6;
inline int32_t KING_SAFE_CHECK_BONUS = 11;
inline int32_t KING_FORCING_CHECK_BONUS = 6;
inline int32_t KING_ATTACK_DANGER_CAP = 112;

// Magic number constants for king safety (tunable)
inline int32_t KING_SHELTER_PAWN_MULTIPLIER = 12;             // Shield pawn penalty per missing
inline int32_t KING_ATTACK_QUEEN_WEIGHT = 34;                 // Material scale: queen
inline int32_t KING_ATTACK_ROOK_WEIGHT = 16;                  // Material scale: rook
inline int32_t KING_ATTACK_MINOR_WEIGHT = 8;                  // Material scale: minor
inline int32_t KING_ATTACK_OPEN_FILE_INCREMENT = 4;           // Material scale: open file
inline int32_t KING_ATTACK_HEAVY_FILE_INCREMENT = 8;          // Material scale: heavy piece on file
inline int32_t KING_SHELTER_INIT_DISTANCE = 99;               // Initial shelter distance (invalid marker)
inline int32_t KING_SHELTER_VERY_CLOSE = 1;                   // Pawn 1 square in front
inline int32_t KING_SHELTER_CLOSE = 2;                        // Pawn 2 squares in front
inline int32_t KING_SHELTER_FAR = 3;                          // Pawn 3 squares in front
inline int32_t KING_SHELTER_MIN_ADVANCE_CHECK = 2;            // Min distance for advance penalties
inline int32_t KING_SHELTER_ADVANCE_PAWN_MULTIPLIER = 2;      // Storm advance penalty multiplier

// ===================================================
// STALEMATE HANDLING
// ===================================================
inline int32_t STALEMATE_DRAW_PENALTY_MAJOR = 450;
inline int32_t STALEMATE_DRAW_PENALTY_MINOR = 120;
inline int32_t STALEMATE_MATERIAL_THRESHOLD = 130;

// ===================================================
// MOVE ORDERING (SEARCH)
// ===================================================
inline int32_t CHECK_BONUS = 50;
inline int32_t KILLER1_BONUS = 2000;
inline int32_t KILLER2_BONUS = 1900;

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
