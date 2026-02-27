#ifndef ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
#define ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP

namespace engine {

// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
inline static constexpr int64_t KING_SAFETY_PENALTY = -12;  // was -18 (too high, accumulated to exceed pawn value)
inline static constexpr int64_t KING_ACTIVITY_BONUS = 8;

inline static constexpr int64_t CASTLE_PAWN_SUPPORT_BONUS = 8;

// REDUCED: Was -40, which meant 3 enemies near king = -120cp swing
// That's MORE than a pawn! Engine would sacrifice pawns to "attack" the king
// New value: 3 enemies = 75cp, still significant but < 1 pawn
inline static constexpr int64_t KING_EXPOSED_PENALTY = -25;

// REDUCED: Moving king early is bad but not worth a pawn
inline static constexpr int64_t EARLY_KING_PENALTY = -20;    // was -25

// King attack zone: bonus for each attacker type near the enemy king
// With quadratic scaling, doubled values were giving 4x the intended bonus:
// 2 attackers with doubled weights: (4 * (40+80))/8 = 60cp
// 2 attackers with original weights: (4 * (20+40))/8 = 30cp
// The ORIGINAL values + quadratic already gave correct attack incentive!
//
// Formula: (attackerCount^2 * totalWeight) / 12
// Divisor increased from 8 to 12 for additional safety margin.
// This ensures multi-piece attacks are rewarded but NEVER exceed pawn value
// with just 2 attackers.

// knight near enemy king - ORIGINAL value
inline static constexpr int64_t KING_ATTACK_WEIGHT_KNIGHT = 20;

// bishop attacking king zone - ORIGINAL value
inline static constexpr int64_t KING_ATTACK_WEIGHT_BISHOP = 20;

// rook attacking king zone - ORIGINAL value
inline static constexpr int64_t KING_ATTACK_WEIGHT_ROOK   = 40;

// queen attacking king zone - ORIGINAL value
inline static constexpr int64_t KING_ATTACK_WEIGHT_QUEEN  = 80;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
