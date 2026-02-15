#ifndef ENGINE_BONUS_MALUS_HANGING_HPP
#define ENGINE_BONUS_MALUS_HANGING_HPP

namespace engine {

// ===================================================
// HANGING PIECES (CRITICAL - balance with SEE and move ordering!)
// IMPORTANTE: NON devono essere troppo alte altrimenti l'engine ha paura di catturare!
// La SEE nella search già valuta gli scambi, quindi qui serve solo un "warning"
// BUGFIX: Ridotte ulteriormente per evitare doppia contabilità con SEE
// L'engine non deve "giustificare" catture losing con "rimozione hanging penalty"
// ===================================================
// TUNED: Reduced from -30 to -20 (SEE already handles material exchanges)
inline static constexpr int64_t HANGING_PAWN_PENALTY   = -20;

// TUNED: Reduced from -55 to -40 (avoid double-counting with SEE)
inline static constexpr int64_t HANGING_MINOR_PENALTY  = -40;

// TUNED: Reduced from -85 to -60
inline static constexpr int64_t HANGING_ROOK_PENALTY   = -60;

// TUNED: Reduced from -140 to -100
inline static constexpr int64_t HANGING_QUEEN_PENALTY  = -100;

// Pawn-specific penalties (additional checks beyond hanging)
// ridotto da -25
inline static constexpr int64_t UNDEFENDED_PAWN_PENALTY = -15;

// ridotto da -15
inline static constexpr int64_t ATTACKED_PAWN_PENALTY = -8;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_HANGING_HPP
