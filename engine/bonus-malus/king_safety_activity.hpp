#ifndef ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
#define ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP

namespace engine {

// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
inline static constexpr int32_t KING_SAFETY_PENALTY = -12;  // was -18 (too high, accumulated to exceed pawn value)
inline static constexpr int32_t KING_ACTIVITY_BONUS = 8;

inline static constexpr int32_t CASTLE_PAWN_SUPPORT_BONUS = 8;

// Shelter/storm model around king file and adjacent files.
inline static constexpr int32_t KING_SHELTER_STRONG_BONUS = 12;
inline static constexpr int32_t KING_SHELTER_WEAK_BONUS = 7;
inline static constexpr int32_t KING_SHELTER_MISSING_PENALTY = 14;
inline static constexpr int32_t KING_PAWN_STORM_NEAR_PENALTY = 16;
inline static constexpr int32_t KING_PAWN_STORM_FAR_PENALTY = 8;
inline static constexpr int32_t KING_CASTLED_SHIELD_BREAK_PENALTY = 10;
inline static constexpr int32_t KING_SHELTER_ADVANCE_ONE_PENALTY = 4;
inline static constexpr int32_t KING_SHELTER_ADVANCE_TWO_PENALTY = 9;
inline static constexpr int32_t KING_HOOK_PAWN_ATTACKED_PENALTY = 20;
inline static constexpr int32_t KING_HOOK_PAWN_HANGING_PENALTY = 25;
inline static constexpr int32_t KING_SAFETY_OPENING_SCALE_PERCENT = 35;

// Open files/diagonals toward king.
inline static constexpr int32_t KING_SEMI_OPEN_FILE_PENALTY = 10;
inline static constexpr int32_t KING_OPEN_FILE_PENALTY = 16;
inline static constexpr int32_t KING_FILE_PRESSURE_PENALTY = 9;
inline static constexpr int32_t KING_OPEN_DIAGONAL_PENALTY = 14;

// REDUCED: Was -40, which meant 3 enemies near king = -120cp swing
// That's MORE than a pawn! Engine would sacrifice pawns to "attack" the king
// New value: 3 enemies = 75cp, still significant but < 1 pawn
inline static constexpr int32_t KING_EXPOSED_PENALTY = -25;

// REDUCED: Moving king early is bad but not worth a pawn
inline static constexpr int32_t EARLY_KING_PENALTY = -20;    // was -25

// King attack zone weights used as attack "units".
// Final danger is scaled by attacker-count saturation in eval_king.cpp.

inline static constexpr int32_t KING_ATTACK_WEIGHT_KNIGHT = 10;
inline static constexpr int32_t KING_ATTACK_WEIGHT_BISHOP = 10;
inline static constexpr int32_t KING_ATTACK_WEIGHT_ROOK   = 18;
inline static constexpr int32_t KING_ATTACK_WEIGHT_QUEEN  = 27;

// Contact pressure and checks in king-zone attack model.
inline static constexpr int32_t KING_SAFE_CONTACT_BONUS = 6;
inline static constexpr int32_t KING_FORCING_CONTACT_BONUS = 3;
inline static constexpr int32_t KING_SAFE_CHECK_BONUS = 12;
inline static constexpr int32_t KING_FORCING_CHECK_BONUS = 5;
inline static constexpr int32_t KING_ATTACK_DANGER_CAP = 145;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
