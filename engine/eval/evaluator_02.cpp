#include "evaluator.hpp"
#include "../piecevaluetables.hpp"

namespace engine {

inline int64_t Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
    int64_t score = 0;

    const int sign = (color == 0) ? 1 : -1;
    const int targetRank = (color == 0) ? 6 : 1;

    if (!rooks) {
      return score;
    }
    const int sq = popLSB(rooks);

    const int file = sq & 7;
    const int rank = sq >> 3;
    const uint64_t fm = FILE_MASKS[file];
    const bool ownPawnOnFile = (ownPawns & fm) != 0;
    const bool oppPawnOnFile = (oppPawns & fm) != 0;
    const int64_t fileBonus = (!ownPawnOnFile) * ((!oppPawnOnFile) ? OPEN_FILE_ROOK_BONUS : SEMI_OPEN_FILE_ROOK_BONUS) * sign;
    score += fileBonus + (rank == targetRank) * (sign * ROOK_ON_SEVENTH_BONUS);

    if (!rooks) {
      return score;
    }

    const int sq2 = popLSB(rooks);
    const int file2 = sq2 & 7;
    const int rank2 = sq2 >> 3;
    const uint64_t fm2 = FILE_MASKS[file2];
    const bool ownPawnOnFile2 = (ownPawns & fm2) != 0;
    const bool oppPawnOnFile2 = (oppPawns & fm2) != 0;
    const int64_t fileBonus2 = (!ownPawnOnFile2) * ((!oppPawnOnFile2) ? OPEN_FILE_ROOK_BONUS : SEMI_OPEN_FILE_ROOK_BONUS) * sign;
    score += fileBonus2 + (rank2 == targetRank) * (sign * ROOK_ON_SEVENTH_BONUS);

    return score;
}

int64_t Evaluator::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const int64_t whiteRookScore = Evaluator::evalRooksForColor(0, whiteRooks, whitePawns, blackPawns);
    const int64_t blackRookScore = Evaluator::evalRooksForColor(1, blackRooks, blackPawns, whitePawns);

    return whiteRookScore + blackRookScore;
}

inline int64_t Evaluator::evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept {
    int64_t score = 0;
    const int sign = (color == 0) ? -1 : 1;

    uint64_t minors = b.knights_bb[color] | b.bishops_bb[color];
    if (!minors) {
      return score;
    }


    const uint64_t friends = b.pawns_bb[color] | b.knights_bb[color] | b.bishops_bb[color] | b.rooks_bb[color] | b.queens_bb[color];
    while (minors) {
        const int sq = popLSB(minors);
        const uint64_t nearby = KING_PROXIMITY_MASKS[sq];
        if ((friends & nearby) == 0) {
            score += sign * COORDINATION_PENALTY;
        }
    }
    return score;
}

int64_t Evaluator::evalPieceCoordination(const chess::Board& b) noexcept {
    const int64_t whiteCoordinationScore = Evaluator::evalPieceCoordinationForColor(b, 0);
    const int64_t blackCoordinationScore = Evaluator::evalPieceCoordinationForColor(b, 1);

    return whiteCoordinationScore + blackCoordinationScore;
}

inline int64_t Evaluator::evalOutpostsForColor(const chess::Board& b, int color) noexcept {
    int64_t score = 0;
    const int sign = (color == 0) ? 1 : -1;
    const int opp = color ^ 1;

    uint64_t knights = b.knights_bb[color];
    while (knights) {
        const int sq = popLSB(knights);
        const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[color][sq] & b.pawns_bb[color]) != 0;
        const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[opp][sq] & b.pawns_bb[opp]) != 0;
        if (supportedByPawn && !attackedByEnemyPawn) {
            score += sign * OUTPOST_KNIGHT_BONUS;
        }
    }

    uint64_t bishops = b.bishops_bb[color];
    while (bishops) {
        const int sq = popLSB(bishops);
        const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[color][sq] & b.pawns_bb[color]) != 0;
        const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[opp][sq] & b.pawns_bb[opp]) != 0;
        if (supportedByPawn && !attackedByEnemyPawn) {
            score += sign * (OUTPOST_BISHOP_BONUS / 2);
        }
    }

    return score;
}

int64_t Evaluator::evalOutposts(const chess::Board& b) noexcept {
    const int64_t whiteOutpostScore = Evaluator::evalOutpostsForColor(b, 0);
    const int64_t blackOutpostScore = Evaluator::evalOutpostsForColor(b, 1);

    return whiteOutpostScore + blackOutpostScore;
}


} // namespace engine
