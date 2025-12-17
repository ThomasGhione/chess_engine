#ifndef ENGINE_BASEBONUSPENALTYVALUES_HPP
#define ENGINE_BASEBONUSPENALTYVALUES_HPP

#include <cstdint>

namespace engine {

// BASE VALUE
inline static constexpr int64_t PAWN_VALUE   =       100;
inline static constexpr int64_t KNIGHT_VALUE =       320;
inline static constexpr int64_t BISHOP_VALUE =       330;
inline static constexpr int64_t ROOK_VALUE   =       500;
inline static constexpr int64_t QUEEN_VALUE  =       900;
inline static constexpr int64_t KING_VALUE   =    20'000;
inline static constexpr int64_t MATE_SCORE   = 1'000'000;

// GENERAL EVALUATION CONSTANTS
inline static constexpr int64_t DEVELOPMENT_BONUS = 10;
inline static constexpr int64_t PINNED_PIECE_PENALTY = -50; // per ogni pezzo pinnato
inline static constexpr int64_t MOBILITY_BONUS = 5; // per ogni mossa legale disponibile

// PAWN
inline static constexpr int64_t PAWN_CLOSE_TO_PROMOTION_BONUS = 10;
inline static constexpr int64_t CENTER_CONTROL_BONUS = 20;
inline static constexpr int64_t DOUBLED_PAWN_PENALTY = -15;
inline static constexpr int64_t ISOLATED_PAWN_PENALTY = -15;
inline static constexpr int64_t BACKWARD_PAWN_PENALTY = -10;
inline static constexpr int64_t TROUBLED_PAWN_PENALTY = -20; // per ogni pedone sotto attacco senza difesa
inline static constexpr int64_t PASSED_PAWN_BONUS = 25; // per ogni pedone passato

// KNIGHT
inline static constexpr int64_t OUTPOST_KNIGHT_BONUS = 30; // per ogni cavallo in outpost

// BISHOP
inline static constexpr int64_t BISHOP_PAIR_BONUS = 10;

// ROOK
inline static constexpr int64_t OPEN_FILE_ROOK_BONUS = 20;
inline static constexpr int64_t SEMI_OPEN_FILE_ROOK_BONUS = 10;
//TODO forse non vale sempre?
inline static constexpr int64_t ROOK_ON_SEVENTH_BONUS = 40; // per ogni torre sulla settima traversa


// QUEEN
inline static constexpr int64_t ATTACKED_QUEEN_PENALTY = -30;

// KING
// inline static constexpr int64_t CASTLING_BONUS = 30;
inline static constexpr int64_t CASTLE_PAWN_SUPPORT_BONUS = 5; // per ogni pedone che supporta il re
inline static constexpr int64_t KING_SAFETY_PENALTY = -50; // per ogni pezzo avversario vicino al re
inline static constexpr int64_t KING_ACTIVITY_BONUS = 10; // per ogni pezzo amico vicino al re in fase finale

// GAME PHASE
inline static constexpr int64_t PHASE_FINAL_THRESHOLD = 8; // soglia per considerare la fase finale (numero totale di pezzi minori e regine)


static constexpr int64_t CHECK_BONUS                 = 50;       // bonus per dare scacco
static constexpr int64_t KILLER1_BONUS               = 2000;   // bonus killer move primaria
static constexpr int64_t KILLER2_BONUS               = 1900;    // bonus killer move secondaria
static constexpr int64_t KING_NON_CASTLING_PENALTY   = 80;    // penalita' per muovere il re senza arroccare
static constexpr int64_t CASTLING_BONUS              = 90;     // BONUS ARROCCO (molto alto per priorita')


static constexpr int64_t HANGING_PAWN_PENALTY   = -40;
static constexpr int64_t HANGING_MINOR_PENALTY = -90;
static constexpr int64_t HANGING_ROOK_PENALTY  = -180;
static constexpr int64_t HANGING_QUEEN_PENALTY = -350;



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