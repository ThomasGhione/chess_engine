#ifndef ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
#define ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP

namespace engine {

// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
// CRITICAL: Increased penalties - king safety is paramount!
inline static constexpr int64_t KING_SAFETY_PENALTY = -18;  // was -10 (too weak)
inline static constexpr int64_t KING_ACTIVITY_BONUS = 8;

inline static constexpr int64_t CASTLE_PAWN_SUPPORT_BONUS = 8;

// INCREASED: exposed king is dangerous!
inline static constexpr int64_t KING_EXPOSED_PENALTY = -40;  // was -25

// INCREASED: moving king early is risky!
inline static constexpr int64_t EARLY_KING_PENALTY = -25;    // was -15

// King attack zone: bonus for each attacker type near the enemy king
// CRITICAL: INCREASED to incentivize king attacks!
// Engine must understand that attacking the king is PRIMARY goal
// Scaled QUADRATICALLY: 2 attackers is 4x as dangerous as 1, 3 attackers is 9x as dangerous
// Formula: (attackerCount^2 * totalWeight) / 8
// This incentivizes coordinated multi-piece attacks over perpetual checks

// knight near enemy king - DOUBLED from 20
inline static constexpr int64_t KING_ATTACK_WEIGHT_KNIGHT = 40;

// bishop attacking king zone - DOUBLED from 20
inline static constexpr int64_t KING_ATTACK_WEIGHT_BISHOP = 40;

// rook attacking king zone - DOUBLED from 40
inline static constexpr int64_t KING_ATTACK_WEIGHT_ROOK   = 80;

// queen attacking king zone - DOUBLED from 80
inline static constexpr int64_t KING_ATTACK_WEIGHT_QUEEN  = 160;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_KING_SAFETY_ACTIVITY_HPP
