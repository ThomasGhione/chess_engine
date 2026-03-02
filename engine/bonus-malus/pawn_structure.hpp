#ifndef ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP
#define ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP

namespace engine {

// ===================================================
// PAWN STRUCTURE EVALUATION
// ===================================================
inline static constexpr int64_t DOUBLED_PAWN_PENALTY = -8;   // was -12 (too high)

inline static constexpr int64_t ISOLATED_PAWN_PENALTY = -12; // was -18 (too high)

// Passed pawns are valuable but not worth piece sacrifices
inline static constexpr int64_t PASSED_PAWN_BONUS = 32;

// Pawn islands: each island after the first is a structural weakness.
inline static constexpr int64_t PAWN_ISLAND_PENALTY = -10;

// Pawn support and dynamic pawn race terms.
inline static constexpr int64_t PAWN_SUPPORT_BONUS = 15;
inline static constexpr int64_t CANDIDATE_PASSER_BONUS = 12;
inline static constexpr int64_t CONNECTED_PASSER_BONUS = 18;
inline static constexpr int64_t BACKWARD_PAWN_PENALTY = -10;
inline static constexpr int64_t PASSED_PAWN_BLOCKED_PENALTY = -16;

inline static constexpr int64_t CENTER_CONTROL_BONUS = 15;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_PAWN_STRUCTURE_HPP
