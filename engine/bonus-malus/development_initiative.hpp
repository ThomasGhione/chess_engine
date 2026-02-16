#ifndef ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP
#define ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP

namespace engine {

// ===================================================
// DEVELOPMENT & INITIATIVE
// ===================================================
// BALANCED: Initiative and development help but must NOT justify pawn sacrifices
// The sum of all positional bonuses for a "well-placed" side must stay WELL below
// 100cp (pawn value) to prevent speculative sacrifices.

// Tempo bonus mid-game - REDUCED from 10 to 6
// 10cp per move meant the engine saw "I have the initiative" as worth 10cp
// Combined with development and king attack, this exceeded pawn value
inline static constexpr int64_t INIT_BONUS_MG = 6;

// bonus iniziativa end-game
inline static constexpr int64_t INIT_BONUS_EG = 3;

inline static constexpr int64_t EARLY_ROOK_PENALTY = -30;

// REDUCED from 12 to 8: full development (4 pieces) = 32cp instead of 48cp
// 48cp was almost half a pawn, making the engine think "sacrifice a pawn for
// fast development" was a good trade
inline static constexpr int64_t DEVELOPMENT_BONUS = 10;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP
