#ifndef ENGINE_BONUS_MALUS_PIECE_BASE_VALUES_HPP
#define ENGINE_BONUS_MALUS_PIECE_BASE_VALUES_HPP

#include <limits>

namespace engine {

// ===================================================
// PIECE BASE VALUES
// ===================================================
inline static constexpr int32_t PAWN_VALUE   =       100;
inline static constexpr int32_t KNIGHT_VALUE =       320;
inline static constexpr int32_t BISHOP_VALUE =       330;
inline static constexpr int32_t ROOK_VALUE   =       500;
inline static constexpr int32_t QUEEN_VALUE  =       900;
inline static constexpr int32_t KING_VALUE   =    20'000;
inline static constexpr int32_t MATE_SCORE   = std::numeric_limits<int32_t>::max();

} // namespace engine

#endif // ENGINE_BONUS_MALUS_PIECE_BASE_VALUES_HPP
