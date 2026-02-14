#ifndef ENGINE_BONUS_MALUS_STALEMATE_HANDLING_HPP
#define ENGINE_BONUS_MALUS_STALEMATE_HANDLING_HPP

namespace engine {

// ===================================================
// STALEMATE HANDLING
// ===================================================
// Small draw bias (centipawns) used to prefer wins over draws when ahead in material.
// Inspired by Stockfish approach: treat draw as slightly worse for the side with material advantage.

// 4 pawn-equivalents (centipawns)
inline static constexpr int64_t STALEMATE_DRAW_PENALTY_MAJOR = 400;

// 1 pawn-equivalent
inline static constexpr int64_t STALEMATE_DRAW_PENALTY_MINOR = 100;

// 1.5 pawns (catch smaller advantages)
inline static constexpr int64_t STALEMATE_MATERIAL_THRESHOLD = 150;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_STALEMATE_HANDLING_HPP
