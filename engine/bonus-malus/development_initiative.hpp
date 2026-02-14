#ifndef ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP
#define ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP

namespace engine {

// ===================================================
// DEVELOPMENT & INITIATIVE
// ===================================================

// bonus iniziativa mid-game (aumentato)
inline static constexpr int64_t INIT_BONUS_MG = 15;

// bonus iniziativa end-game
inline static constexpr int64_t INIT_BONUS_EG = 3;

// RADDOPPIATO per evitare torre troppo presto
inline static constexpr int64_t EARLY_ROOK_PENALTY = -30;

// TUNED: was 15, too high (causes tactical blindness)
inline static constexpr int64_t DEVELOPMENT_BONUS = 10;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_DEVELOPMENT_INITIATIVE_HPP
