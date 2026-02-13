#ifndef ENGINE_BASEBONUSPENALTYVALUES_HPP
#define ENGINE_BASEBONUSPENALTYVALUES_HPP

#include <cstdint>

namespace engine {

// ===================================================
// PIECE BASE VALUES
// ===================================================
inline static constexpr int64_t PAWN_VALUE   =       100;
inline static constexpr int64_t KNIGHT_VALUE =       320;
inline static constexpr int64_t BISHOP_VALUE =       330;
inline static constexpr int64_t ROOK_VALUE   =       500;
inline static constexpr int64_t QUEEN_VALUE  =       900;
inline static constexpr int64_t KING_VALUE   =    20'000;
inline static constexpr int64_t MATE_SCORE   = 1'000'000;

// ===================================================
// PAWN STRUCTURE EVALUATION
// ===================================================
inline static constexpr int64_t DOUBLED_PAWN_PENALTY = -12;      // TUNED: was -20 (too harsh, caused avoidance of good pawn structures)
inline static constexpr int64_t ISOLATED_PAWN_PENALTY = -18;     // aumentato
// BACKWARD_PAWN_PENALTY removed - was never implemented in evalPawnStructure()
inline static constexpr int64_t PASSED_PAWN_BONUS = 32;          // REDUCED from 40 (was too high, caused bad sacrifices)
inline static constexpr int64_t CENTER_CONTROL_BONUS = 15;       // REDUCED from 25 (was too high)

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
inline static constexpr int64_t OUTPOST_BISHOP_BONUS = 20;               // centipawns
inline static constexpr int64_t OUTPOST_KNIGHT_BONUS = 30;        // knight outposts are more valuable

// Move-ordering penalty for moving the same pawn again during opening
// (search-time penalty applied in move ordering; negative number lowers priority)
inline static constexpr int64_t ORDERING_PENALTY_SAME_PAWN_OPENING = -15;

// ===================================================
// HANGING PIECES (CRITICAL - balance with SEE and move ordering!)
// IMPORTANTE: NON devono essere troppo alte altrimenti l'engine ha paura di catturare!
// La SEE nella search già valuta gli scambi, quindi qui serve solo un "warning"
// ===================================================
inline static constexpr int64_t HANGING_PAWN_PENALTY   = -30;   // ridotto! (era -90, troppo alto)
inline static constexpr int64_t HANGING_MINOR_PENALTY  = -55;   // TUNED: was -80 (too punitive, SEE already handles exchanges)
inline static constexpr int64_t HANGING_ROOK_PENALTY   = -85;   // TUNED: was -120 (too punitive)
inline static constexpr int64_t HANGING_QUEEN_PENALTY  = -140;  // TUNED: was -200 (too punitive)

// Pawn-specific penalties (additional checks beyond hanging)
inline static constexpr int64_t UNDEFENDED_PAWN_PENALTY = -15;  // ridotto da -25
inline static constexpr int64_t ATTACKED_PAWN_PENALTY = -8;     // ridotto da -15

// ===================================================
// ROOK EVALUATION
// ===================================================
inline static constexpr int64_t OPEN_FILE_ROOK_BONUS = 30;
inline static constexpr int64_t SEMI_OPEN_FILE_ROOK_BONUS = 15;
inline static constexpr int64_t ROOK_ON_SEVENTH_BONUS = 25;

// ===================================================
// QUEEN EVALUATION
// ===================================================
inline static constexpr int64_t ATTACKED_QUEEN_PENALTY = -25;    // ridotto da -30

// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
inline static constexpr int64_t KING_SAFETY_PENALTY = -10;       // ridotto ulteriormente
inline static constexpr int64_t KING_ACTIVITY_BONUS = 8;         
inline static constexpr int64_t CASTLE_PAWN_SUPPORT_BONUS = 8;   // FIX: was 4 (too low), now symmetric for both sides
inline static constexpr int64_t KING_EXPOSED_PENALTY = -25;      // ridotto da -40
inline static constexpr int64_t EARLY_KING_PENALTY = -15;        // ridotto da -20

// King attack zone: bonus for each attacker type near the enemy king
// Scaled QUADRATICALLY: 2 attackers is 4x as dangerous as 1, 3 attackers is 9x as dangerous
// Formula: (attackerCount^2 * totalWeight) / 8
// This incentivizes coordinated multi-piece attacks over perpetual checks
inline static constexpr int64_t KING_ATTACK_WEIGHT_KNIGHT = 20;  // knight near enemy king
inline static constexpr int64_t KING_ATTACK_WEIGHT_BISHOP = 20;  // bishop attacking king zone
inline static constexpr int64_t KING_ATTACK_WEIGHT_ROOK   = 40;  // rook attacking king zone
inline static constexpr int64_t KING_ATTACK_WEIGHT_QUEEN  = 80;  // queen attacking king zone

// ===================================================
// CASTLING
// ===================================================
inline static constexpr int64_t CASTLING_BONUS = 35;             // aumentato leggermente (l'arrocco è importante!)
inline static constexpr int64_t KING_NON_CASTLING_PENALTY = 10;  // TUNED: was 20 (too high, redundant with evalCastlingBonus)

// ===================================================
// DEVELOPMENT & INITIATIVE
// ===================================================
inline static constexpr int64_t INIT_BONUS_MG = 15;    // bonus iniziativa mid-game (aumentato)
inline static constexpr int64_t INIT_BONUS_EG = 3;     // bonus iniziativa end-game
inline static constexpr int64_t EARLY_ROOK_PENALTY = -30;  // RADDOPPIATO per evitare torre troppo presto
inline static constexpr int64_t DEVELOPMENT_BONUS = 10;    // TUNED: was 15, too high (causes tactical blindness)

// ===================================================
// MOVE ORDERING (SEARCH)
// ===================================================
static constexpr int64_t CHECK_BONUS = 50;        
static constexpr int64_t KILLER1_BONUS = 2000;   
static constexpr int64_t KILLER2_BONUS = 1900;

// ===================================================
// BISHOP PAIR
// ===================================================
inline static constexpr int64_t BISHOP_PAIR_BONUS = 30;     // aumentato (coppia alfieri importante)

// ===================================================
// ROOK ENDGAME (R+K vs K)
// Strategy: push enemy king to edge to deliver checkmate
// ===================================================
inline static constexpr int64_t ROOK_EG_EDGE_BONUS = 35;        // bonus when opponent king is near edge
inline static constexpr int64_t ROOK_EG_PRESSURE_BONUS = 20;    // bonus for active coordination

// ===================================================
// GAME PHASE
// ===================================================
inline static constexpr int64_t PHASE_FINAL_THRESHOLD = 8;

// ===================================================
// STALEMATE HANDLING
// ===================================================
// Small draw bias (centipawns) used to prefer wins over draws when ahead in material.
// Inspired by Stockfish approach: treat draw as slightly worse for the side with material advantage.
inline static constexpr int64_t STALEMATE_DRAW_PENALTY_MAJOR = 400; // 4 pawn-equivalents (centipawns)
inline static constexpr int64_t STALEMATE_DRAW_PENALTY_MINOR = 100; // 1 pawn-equivalent
inline static constexpr int64_t STALEMATE_MATERIAL_THRESHOLD = 150; // 1.5 pawns (catch smaller advantages)


// MVV (Most Valuable Victim) table for capture ordering
// Simplified from MVV-LVA: only victim value matters (attacker irrelevant)
// SEE already handles exchange evaluation, so MVV-only is sufficient
// Indices: 0=EMPTY, 1=PAWN, 2=KNIGHT, 3=BISHOP, 4=ROOK, 5=QUEEN, 6=KING
inline constexpr int64_t MVV_TABLE[7] = {
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
inline constexpr int64_t MVV_LVA_TABLE[7][7] = {
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
    // victim: KING (20000) - teoricamente non dovrebbe essere catturato, ma per completezza
    {0, KING_VALUE*10 - PAWN_VALUE, KING_VALUE*10 - KNIGHT_VALUE, KING_VALUE*10 - BISHOP_VALUE, KING_VALUE*10 - ROOK_VALUE, KING_VALUE*10 - QUEEN_VALUE, KING_VALUE*10 - KING_VALUE}
};



}

#endif // ENGINE_BASEBONUSPENALTYVALUES_HPP