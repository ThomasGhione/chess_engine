#ifndef ENGINE_BONUS_MALUS_CASTLING_HPP
#define ENGINE_BONUS_MALUS_CASTLING_HPP


namespace engine {

// ===================================================
// CASTLING
// ===================================================

// Slightly increased (castling is important)
inline static constexpr int64_t CASTLING_BONUS = 35;

// TUNED: was 20 (too high, redundant with evalCastlingBonus)
inline static constexpr int64_t KING_NON_CASTLING_PENALTY = 10;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_CASTLING_HPP
