#ifndef ENGINE_BONUS_MALUS_ROOK_ENDGAME_HPP
#define ENGINE_BONUS_MALUS_ROOK_ENDGAME_HPP

namespace engine {

// ===================================================
// ROOK ENDGAME (R+K vs K)
// Strategy: push enemy king to edge to deliver checkmate
// ===================================================

// bonus when opponent king is near edge
inline static constexpr int32_t ROOK_EG_EDGE_BONUS = 35;

// bonus for active coordination
inline static constexpr int32_t ROOK_EG_PRESSURE_BONUS = 20;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_ROOK_ENDGAME_HPP
