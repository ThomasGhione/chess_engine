#ifndef ENGINE_BONUS_MALUS_HANGING_HPP
#define ENGINE_BONUS_MALUS_HANGING_HPP

namespace engine {

// ===================================================
// HANGING PIECES (CRITICAL - balance with SEE and move ordering!)
// ===================================================

inline static constexpr int64_t HANGING_PAWN_PENALTY   = -12;
inline static constexpr int64_t HANGING_PAWN_NEAR_KING_PENALTY = -28;
inline static constexpr int64_t HANGING_HOOK_PAWN_PENALTY = -16;

inline static constexpr int64_t HANGING_MINOR_PENALTY  = -25;

inline static constexpr int64_t HANGING_ROOK_PENALTY   = -35;

inline static constexpr int64_t HANGING_QUEEN_PENALTY  = -50;

// Pawn-specific penalties (additional checks beyond hanging)
inline static constexpr int64_t UNDEFENDED_PAWN_PENALTY = -10;

inline static constexpr int64_t ATTACKED_PAWN_PENALTY = -5;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_HANGING_HPP
