#ifndef ENGINE_BONUS_MALUS_CASTLING_HPP
#define ENGINE_BONUS_MALUS_CASTLING_HPP


namespace engine {

// ===================================================
// CASTLING
// ===================================================

inline static constexpr int64_t CASTLING_BONUS = 35;

inline static constexpr int64_t KING_NON_CASTLING_PENALTY = 10;
inline static constexpr int64_t KING_LOST_CASTLING_RIGHTS_PENALTY = 50;
inline static constexpr int64_t LOSS_OF_CASTLING_PENALTY = 120;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_CASTLING_HPP
