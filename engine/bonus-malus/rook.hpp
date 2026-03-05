#ifndef ENGINE_BONUS_MALUS_ROOK_HPP
#define ENGINE_BONUS_MALUS_ROOK_HPP

namespace engine {

// ===================================================
// ROOK EVALUATION
// ===================================================
inline static constexpr int32_t OPEN_FILE_ROOK_BONUS = 30;
inline static constexpr int32_t SEMI_OPEN_FILE_ROOK_BONUS = 15;
inline static constexpr int32_t ROOK_ON_SEVENTH_BONUS = 25;
inline static constexpr int32_t ROOK_BEHIND_OWN_PASSER_BONUS = 18;
inline static constexpr int32_t ROOK_BEHIND_ENEMY_PASSER_BONUS = 14;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_ROOK_HPP
