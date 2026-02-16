#ifndef ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP
#define ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP

namespace engine {

// ===================================================
// PAWN STRUCTURE EVALUATION
// ===================================================
// CRITICAL FIX: Reduced penalties - prevent material sacrifices for pawn structure
// Engine was sacrificing material to "fix" opponent's pawn structure
inline static constexpr int64_t DOUBLED_PAWN_PENALTY = -8;   // was -12 (too high)

inline static constexpr int64_t ISOLATED_PAWN_PENALTY = -12; // was -18 (too high)

// Passed pawns are valuable but not worth piece sacrifices
inline static constexpr int64_t PASSED_PAWN_BONUS = 32;

inline static constexpr int64_t CENTER_CONTROL_BONUS = 15;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP
