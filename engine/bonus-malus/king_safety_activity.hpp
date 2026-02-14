#ifndef ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
#define ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP

namespace engine {

// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
// ridotto ulteriormente
inline static constexpr int64_t KING_SAFETY_PENALTY = -10;
inline static constexpr int64_t KING_ACTIVITY_BONUS = 8;

// was 4 (too low)
inline static constexpr int64_t CASTLE_PAWN_SUPPORT_BONUS = 8;

// ridotto da -40
inline static constexpr int64_t KING_EXPOSED_PENALTY = -25;

// ridotto da -20
inline static constexpr int64_t EARLY_KING_PENALTY = -15;

// King attack zone: bonus for each attacker type near the enemy king

// Scaled QUADRATICALLY: 2 attackers is 4x as dangerous as 1, 3 attackers is 9x as dangerous
// Formula: (attackerCount^2 * totalWeight) / 8
// This incentivizes coordinated multi-piece attacks over perpetual checks
// knight near enemy king
inline static constexpr int64_t KING_ATTACK_WEIGHT_KNIGHT = 20;

// bishop attacking king zone
inline static constexpr int64_t KING_ATTACK_WEIGHT_BISHOP = 20;

// rook attacking king zone
inline static constexpr int64_t KING_ATTACK_WEIGHT_ROOK   = 40;

// queen attacking king zone
inline static constexpr int64_t KING_ATTACK_WEIGHT_QUEEN  = 80;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
