#ifndef ENGINE_BONUS_MALUS_HANGING_HPP
#define ENGINE_BONUS_MALUS_HANGING_HPP

namespace engine {

// ===================================================
// HANGING PIECES (CRITICAL - balance with SEE and move ordering!)
// IMPORTANTE: NON devono essere troppo alte altrimenti l'engine ha paura di catturare!
// La SEE nella search già valuta gli scambi, quindi qui serve solo un "warning"
// ===================================================
// ridotto! (era -90, troppo alto)
inline static constexpr int64_t HANGING_PAWN_PENALTY   = -30;

// TUNED: was -80 (too punitive, SEE already handles exchanges)
inline static constexpr int64_t HANGING_MINOR_PENALTY  = -55;

// TUNED: was -120 (too punitive)
inline static constexpr int64_t HANGING_ROOK_PENALTY   = -85;

// TUNED: was -200 (too punitive)
inline static constexpr int64_t HANGING_QUEEN_PENALTY  = -140;

// Pawn-specific penalties (additional checks beyond hanging)
// ridotto da -25
inline static constexpr int64_t UNDEFENDED_PAWN_PENALTY = -15;

// ridotto da -15
inline static constexpr int64_t ATTACKED_PAWN_PENALTY = -8;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_HANGING_HPP
