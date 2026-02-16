#ifndef ENGINE_BONUS_MALUS_HANGING_HPP
#define ENGINE_BONUS_MALUS_HANGING_HPP

namespace engine {

// ===================================================
// HANGING PIECES (CRITICAL - balance with SEE and move ordering!)
// CRITICAL FIX: Further reduced to prevent false material compensation
// The engine should NOT think "I sacrifice a pawn to remove hanging penalty"
// SEE already handles tactical exchanges - these are just subtle warnings
// ===================================================
inline static constexpr int64_t HANGING_PAWN_PENALTY   = -12;  // was -20 (reduced)

inline static constexpr int64_t HANGING_MINOR_PENALTY  = -25;  // was -40 (reduced)

inline static constexpr int64_t HANGING_ROOK_PENALTY   = -35;  // was -60 (reduced)

inline static constexpr int64_t HANGING_QUEEN_PENALTY  = -50;  // was -100 (reduced)

// Pawn-specific penalties (additional checks beyond hanging)
inline static constexpr int64_t UNDEFENDED_PAWN_PENALTY = -10; // was -15 (reduced)

inline static constexpr int64_t ATTACKED_PAWN_PENALTY = -5;    // was -8 (reduced)

} // namespace engine

#endif // ENGINE_BONUS_MALUS_HANGING_HPP
