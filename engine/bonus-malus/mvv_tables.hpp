#ifndef ENGINE_BONUS_MALUS_MVV_TABLES_HPP
#define ENGINE_BONUS_MALUS_MVV_TABLES_HPP

#include "piece_base_values.hpp"

namespace engine {

// ===================================================
// MVV (Most Valuable Victim) table for capture ordering
// Simplified from MVV-LVA: only victim value matters (attacker irrelevant)
// SEE already handles exchange evaluation, so MVV-only is sufficient
// Indices: 0=EMPTY, 1=PAWN, 2=KNIGHT, 3=BISHOP, 4=ROOK, 5=QUEEN, 6=KING
inline constexpr int32_t MVV_TABLE[7] = {
    0,                  // EMPTY
    PAWN_VALUE * 10,    // PAWN = 1000
    KNIGHT_VALUE * 10,  // KNIGHT = 3200
    BISHOP_VALUE * 10,  // BISHOP = 3300
    ROOK_VALUE * 10,    // ROOK = 5000
    QUEEN_VALUE * 10,   // QUEEN = 9000
    KING_VALUE * 10     // KING = 200000 (should never be captured)
};

// Legacy MVV-LVA table kept for compatibility (not used in new code)
// TODO: Remove after confirming MVV-only works well
inline constexpr int32_t MVV_LVA_TABLE[7][7] = {
    // victim: EMPTY
    {0, 0, 0, 0, 0, 0, 0},
    // victim: PAWN (100)
    {0, PAWN_VALUE*10 - PAWN_VALUE, PAWN_VALUE*10 - KNIGHT_VALUE, PAWN_VALUE*10 - BISHOP_VALUE, PAWN_VALUE*10 - ROOK_VALUE, PAWN_VALUE*10 - QUEEN_VALUE, PAWN_VALUE*10 - KING_VALUE},
    // victim: KNIGHT (320)
    {0, KNIGHT_VALUE*10 - PAWN_VALUE, KNIGHT_VALUE*10 - KNIGHT_VALUE, KNIGHT_VALUE*10 - BISHOP_VALUE, KNIGHT_VALUE*10 - ROOK_VALUE, KNIGHT_VALUE*10 - QUEEN_VALUE, KNIGHT_VALUE*10 - KING_VALUE},
    // victim: BISHOP (330)
    {0, BISHOP_VALUE*10 - PAWN_VALUE, BISHOP_VALUE*10 - KNIGHT_VALUE, BISHOP_VALUE*10 - BISHOP_VALUE, BISHOP_VALUE*10 - ROOK_VALUE, BISHOP_VALUE*10 - QUEEN_VALUE, BISHOP_VALUE*10 - KING_VALUE},
    // victim: ROOK (500)
    {0, ROOK_VALUE*10 - PAWN_VALUE, ROOK_VALUE*10 - KNIGHT_VALUE, ROOK_VALUE*10 - BISHOP_VALUE, ROOK_VALUE*10 - ROOK_VALUE, ROOK_VALUE*10 - QUEEN_VALUE, ROOK_VALUE*10 - KING_VALUE},
    // victim: QUEEN (900)
    {0, QUEEN_VALUE*10 - PAWN_VALUE, QUEEN_VALUE*10 - KNIGHT_VALUE, QUEEN_VALUE*10 - BISHOP_VALUE, QUEEN_VALUE*10 - ROOK_VALUE, QUEEN_VALUE*10 - QUEEN_VALUE, QUEEN_VALUE*10 - KING_VALUE},
    // victim: KING (20000) - theoretically should never be captured, kept for completeness
    {0, KING_VALUE*10 - PAWN_VALUE, KING_VALUE*10 - KNIGHT_VALUE, KING_VALUE*10 - BISHOP_VALUE, KING_VALUE*10 - ROOK_VALUE, KING_VALUE*10 - QUEEN_VALUE, KING_VALUE*10 - KING_VALUE}
};

} // namespace engine

#endif // ENGINE_BONUS_MALUS_MVV_TABLES_HPP
