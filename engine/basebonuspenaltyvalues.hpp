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
inline static constexpr int64_t DOUBLED_PAWN_PENALTY = -20;      // aumentato per scoraggiare push eccessivo
inline static constexpr int64_t ISOLATED_PAWN_PENALTY = -18;     // aumentato
inline static constexpr int64_t BACKWARD_PAWN_PENALTY = -12;     // aumentato
inline static constexpr int64_t PASSED_PAWN_BONUS = 15;          // ridotto (meno incentivo a pushare)
inline static constexpr int64_t CENTER_CONTROL_BONUS = 25;       // TRIPLICATO! Centro è fondamentale

// ===================================================
// PIECE MOBILITY & TRAPPED PIECES
// ===================================================
inline static constexpr int64_t LOW_MOBILITY_KNIGHT_PENALTY = 5;   // ridotto ulteriormente
inline static constexpr int64_t PINNED_KNIGHT_PENALTY = 30;        // ridotto da 50
inline static constexpr int64_t LOW_MOBILITY_BISHOP_PENALTY = 10;  // ridotto da 15
inline static constexpr int64_t PINNED_BISHOP_PENALTY = 25;        // ridotto da 35
inline static constexpr int64_t LOW_MOBILITY_ROOK_PENALTY = 15;    // ridotto da 25
inline static constexpr int64_t PINNED_ROOK_PENALTY = 20;          // ridotto da 25
inline static constexpr int64_t LOW_MOBILITY_QUEEN_PENALTY = 30;   // ridotto da 50
inline static constexpr int64_t PINNED_QUEEN_PENALTY = 80;         // ridotto da 150

// Coordination penalty: minor pieces (knights/bishops) far from other friendly pieces
// measured within Manhattan distance <= 2 (useful to promote piece coordination)
inline static constexpr int64_t COORDINATION_PENALTY = 12;         // centipawns

// ===================================================
// HANGING PIECES (CRITICAL - balance with SEE and move ordering!)
// IMPORTANTE: NON devono essere troppo alte altrimenti l'engine ha paura di catturare!
// La SEE nella search già valuta gli scambi, quindi qui serve solo un "warning"
// ===================================================
inline static constexpr int64_t HANGING_PAWN_PENALTY   = -30;   // ridotto! (era -90, troppo alto)
inline static constexpr int64_t HANGING_MINOR_PENALTY  = -80;   // ridotto! (era -280, paralizzava l'engine)
inline static constexpr int64_t HANGING_ROOK_PENALTY   = -120;  // ridotto! (era -450, troppo punitivo)
inline static constexpr int64_t HANGING_QUEEN_PENALTY  = -200;  // ridotto! (era -800, eccessivo)

// Pawn-specific penalties (additional checks beyond hanging)
inline static constexpr int64_t UNDEFENDED_PAWN_PENALTY = -15;  // ridotto da -25
inline static constexpr int64_t ATTACKED_PAWN_PENALTY = -8;     // ridotto da -15

// ===================================================
// ROOK EVALUATION
// ===================================================
inline static constexpr int64_t OPEN_FILE_ROOK_BONUS = 15;       // ridotto da 20
inline static constexpr int64_t SEMI_OPEN_FILE_ROOK_BONUS = 8;   // ridotto da 10
inline static constexpr int64_t ROOK_ON_SEVENTH_BONUS = 25;      // ridotto da 40

// ===================================================
// QUEEN EVALUATION
// ===================================================
inline static constexpr int64_t ATTACKED_QUEEN_PENALTY = -25;    // ridotto da -30

// ===================================================
// KING SAFETY & ACTIVITY
// ===================================================
inline static constexpr int64_t KING_SAFETY_PENALTY = -10;       // ridotto ulteriormente
inline static constexpr int64_t KING_ACTIVITY_BONUS = 8;         
inline static constexpr int64_t CASTLE_PAWN_SUPPORT_BONUS = 4;   
inline static constexpr int64_t KING_EXPOSED_PENALTY = -25;      // ridotto da -40
inline static constexpr int64_t EARLY_KING_PENALTY = -15;        // ridotto da -20

// ===================================================
// CASTLING
// ===================================================
inline static constexpr int64_t CASTLING_BONUS = 35;             // aumentato leggermente (l'arrocco è importante!)
inline static constexpr int64_t KING_NON_CASTLING_PENALTY = 20;  // ridotto da 25

// ===================================================
// DEVELOPMENT & INITIATIVE
// ===================================================
inline static constexpr int64_t INIT_BONUS_MG = 15;    // bonus iniziativa mid-game (aumentato)
inline static constexpr int64_t INIT_BONUS_EG = 3;     // bonus iniziativa end-game
inline static constexpr int64_t EARLY_ROOK_PENALTY = -30;  // RADDOPPIATO per evitare torre troppo presto
inline static constexpr int64_t DEVELOPMENT_BONUS = 15;    // RADDOPPIATO! Sviluppo è critico

// ===================================================
// MOVE ORDERING (SEARCH)
// ===================================================
static constexpr int64_t CHECK_BONUS = 50;        
static constexpr int64_t KILLER1_BONUS = 2000;   
static constexpr int64_t KILLER2_BONUS = 1900;

// ===================================================
// KNIGHT & BISHOP POSITIONING
// ===================================================
inline static constexpr int64_t OUTPOST_KNIGHT_BONUS = 30;  // aumentato (cavalli ben piazzati)
inline static constexpr int64_t BISHOP_PAIR_BONUS = 30;     // aumentato (coppia alfieri importante)

// ===================================================
// GAME PHASE
// ===================================================
inline static constexpr int64_t PHASE_FINAL_THRESHOLD = 8;



// MVV-LVA precomputed table: [victim_type][attacker_type]
// Formula: victimValue * 10 - attackerValue
// Indices: 0=EMPTY, 1=PAWN, 2=KNIGHT, 3=BISHOP, 4=ROOK, 5=QUEEN, 6=KING
// Per MVV-LVA usiamo solo 1-6 (EMPTY non cattura nulla, KING non usiamo come victim normalmente)
inline constexpr int64_t MVV_LVA_TABLE[7][7] = {
    // victim: EMPTY
    {0, 0, 0, 0, 0, 0, 0},
    // victim: PAWN (100)
    {0, 100*10 - 100, 100*10 - 320, 100*10 - 330, 100*10 - 500, 100*10 - 900, 100*10 - 20000},
    // victim: KNIGHT (320)
    {0, 320*10 - 100, 320*10 - 320, 320*10 - 330, 320*10 - 500, 320*10 - 900, 320*10 - 20000},
    // victim: BISHOP (330)
    {0, 330*10 - 100, 330*10 - 320, 330*10 - 330, 330*10 - 500, 330*10 - 900, 330*10 - 20000},
    // victim: ROOK (500)
    {0, 500*10 - 100, 500*10 - 320, 500*10 - 330, 500*10 - 500, 500*10 - 900, 500*10 - 20000},
    // victim: QUEEN (900)
    {0, 900*10 - 100, 900*10 - 320, 900*10 - 330, 900*10 - 500, 900*10 - 900, 900*10 - 20000},
    // victim: KING (20000) - teoricamente non dovrebbe essere catturato, ma per completezza
    {0, 20000*10 - 100, 20000*10 - 320, 20000*10 - 330, 20000*10 - 500, 20000*10 - 900, 20000*10 - 20000}
};



}

#endif // ENGINE_BASEBONUSPENALTYVALUES_HPP