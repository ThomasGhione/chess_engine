#ifndef ENGINE_BONUS_MALUS_MOBILITY_HPP
#define ENGINE_BONUS_MALUS_MOBILITY_HPP

namespace engine {

// ===================================================
// PIECE MOBILITY & TRAPPED PIECES
// ===================================================
inline static constexpr int64_t LOW_MOBILITY_KNIGHT_PENALTY = 10;   
inline static constexpr int64_t PINNED_KNIGHT_PENALTY = 30;
inline static constexpr int64_t LOW_MOBILITY_BISHOP_PENALTY = 15;
inline static constexpr int64_t PINNED_BISHOP_PENALTY = 30;
inline static constexpr int64_t LOW_MOBILITY_ROOK_PENALTY = 45;
inline static constexpr int64_t PINNED_ROOK_PENALTY = 80;
inline static constexpr int64_t LOW_MOBILITY_QUEEN_PENALTY = 55;
inline static constexpr int64_t PINNED_QUEEN_PENALTY = 120;

// Coordination penalty: minor pieces (knights/bishops) far from other friendly pieces
// measured within Manhattan distance <= 2 (useful to promote piece coordination)
inline static constexpr int64_t COORDINATION_PENALTY = 12;         // centipawns

// Outpost bonus for stable knight/bishop squares (supported by pawn and not attacked by enemy pawns)
inline static constexpr int64_t OUTPOST_BISHOP_BONUS = 20;        // centipawns
inline static constexpr int64_t OUTPOST_KNIGHT_BONUS = 30;        // knight outposts are more valuable

// Move-ordering penalty for moving the same pawn again during opening
// (search-time penalty applied in move ordering; negative number lowers priority)
inline static constexpr int64_t ORDERING_PENALTY_SAME_PAWN_OPENING = -15;

} // namespace engine

#endif // ENGINE_BONUS_MALUS_MOBILITY_HPP
