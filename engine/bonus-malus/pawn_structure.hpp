#ifndef ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP
#define ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP

namespace engine {

// ===================================================
// PAWN STRUCTURE EVALUATION
// ===================================================
// TUNED: was -20 (too harsh, caused avoidance of good pawn structures)
inline static constexpr int64_t DOUBLED_PAWN_PENALTY = -12;

// aumentato
inline static constexpr int64_t ISOLATED_PAWN_PENALTY = -18;

// REDUCED from 40 (was too high, caused bad sacrifices)
inline static constexpr int64_t PASSED_PAWN_BONUS = 32;

// REDUCED from 25 (was too high)
inline static constexpr int64_t CENTER_CONTROL_BONUS = 15;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP
