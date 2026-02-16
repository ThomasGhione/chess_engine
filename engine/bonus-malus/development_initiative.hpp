#ifndef ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP
#define ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP

namespace engine {

// ===================================================
// DEVELOPMENT & INITIATIVE
// ===================================================
// CRITICAL: Reduced initiative bonus - not worth material sacrifices!

// bonus iniziativa mid-game - REDUCED
inline static constexpr int64_t INIT_BONUS_MG = 10;

// bonus iniziativa end-game
inline static constexpr int64_t INIT_BONUS_EG = 3;

inline static constexpr int64_t EARLY_ROOK_PENALTY = -30;

inline static constexpr int64_t DEVELOPMENT_BONUS = 12;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP
