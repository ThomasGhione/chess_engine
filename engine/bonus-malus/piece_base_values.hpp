#ifndef ENGINE_BONUS_MALUS_PIECE_BASE_VALUES_HPP
#define ENGINE_BONUS_MALUS_PIECE_BASE_VALUES_HPP

namespace engine {

// ===================================================
// PIECE BASE VALUES
// ===================================================
inline static constexpr int64_t PAWN_VALUE   =       100;
inline static constexpr int64_t KNIGHT_VALUE =       320;
inline static constexpr int64_t BISHOP_VALUE =       330;
inline static constexpr int64_t ROOK_VALUE   =       500;
inline static constexpr int64_t QUEEN_VALUE  =       900;
inline static constexpr int64_t KING_VALUE   =    20'000;
inline static constexpr int64_t MATE_SCORE   = 1'000'000;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_PIECE_BASE_VALUES_HPP
