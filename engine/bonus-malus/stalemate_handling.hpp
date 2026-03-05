#ifndef ENGINE_BONUS_MALUS_STALEMATE_HANDLING_HPP
#define ENGINE_BONUS_MALUS_STALEMATE_HANDLING_HPP

namespace engine {

// ===================================================
// STALEMATE HANDLING
// ===================================================
// Small draw bias (centipawns) used to prefer wins over draws when ahead in material.
// Inspired by Stockfish approach: treat draw as slightly worse for the side with material advantage.

// 4.5 pawn-equivalents (centipawns)
inline static constexpr int32_t STALEMATE_DRAW_PENALTY_MAJOR = 450;

// 1.2 pawn-equivalents
inline static constexpr int32_t STALEMATE_DRAW_PENALTY_MINOR = 120;

// 1.3 pawns (catch smaller advantages a bit earlier)
inline static constexpr int32_t STALEMATE_MATERIAL_THRESHOLD = 130;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_STALEMATE_HANDLING_HPP
