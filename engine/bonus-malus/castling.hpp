#ifndef ENGINE_BONUS_MALUS_CASTLING_HPP
#define ENGINE_BONUS_MALUS_CASTLING_HPP


namespace engine {

// ===================================================
// CASTLING
// ===================================================

inline static constexpr int32_t CASTLING_BONUS = 30;

inline static constexpr int32_t KING_NON_CASTLING_PENALTY = 10;
inline static constexpr int32_t KING_LOST_CASTLING_RIGHTS_PENALTY = 25;
inline static constexpr int32_t LOSS_OF_CASTLING_PENALTY = 35;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_CASTLING_HPP
