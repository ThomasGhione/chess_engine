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
inline static constexpr int64_t PHASE_FINAL_THRESHOLD = 14; // soglia per considerare la fase finale (numero totale di pezzi minori e regine)


static constexpr int64_t CHECK_BONUS                 = 50;       // bonus per dare scacco
static constexpr int64_t KILLER1_BONUS               = 200;   // bonus killer move primaria
static constexpr int64_t KILLER2_BONUS               = 190;    // bonus killer move secondaria
static constexpr int64_t KING_NON_CASTLING_PENALTY   = 110;    // penalita' per muovere il re senza arroccare
static constexpr int64_t CASTLING_BONUS              = 165;     // piccolo bonus per arroccare



}

#endif // ENGINE_BASEBONUSPENALTYVALUES_HPP