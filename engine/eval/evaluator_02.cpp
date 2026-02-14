#include "evaluator.hpp"
#include "../piecevaluetables.hpp"
#include <algorithm>
#include <cstring>
namespace engine {

int64_t Evaluator::evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept {
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

    return whiteRookScore + blackRookScore;;
}

int64_t Evaluator::evalPieceCoordination(const chess::Board& b) noexcept {
    int64_t score = 0;

    // White side
    {
        uint64_t minors = b.knights_bb[0] | b.bishops_bb[0];
        if (minors) {
            const uint64_t friends = b.pawns_bb[0] | b.knights_bb[0] | b.bishops_bb[0] | b.rooks_bb[0] | b.queens_bb[0];
            while (minors) {
                const int sq = popLSB(minors);
                const uint64_t nearby = KING_PROXIMITY_MASKS[sq];
                if ((friends & nearby) == 0) {
                    score -= COORDINATION_PENALTY;
                }
            }
        }
    }

    // Black side
    {
        uint64_t minors = b.knights_bb[1] | b.bishops_bb[1];
        if (minors) {
            const uint64_t friends = b.pawns_bb[1] | b.knights_bb[1] | b.bishops_bb[1] | b.rooks_bb[1] | b.queens_bb[1];
            while (minors) {
                const int sq = popLSB(minors);
                const uint64_t nearby = KING_PROXIMITY_MASKS[sq];
                if ((friends & nearby) == 0) {
                    score += COORDINATION_PENALTY;
                }
            }
        }
    }

    return score;
}

int64_t Evaluator::evalOutposts(const chess::Board& b) noexcept {
    int64_t score = 0;

    // White side
    {
        uint64_t knights = b.knights_bb[0];
        while (knights) {
            const int sq = popLSB(knights);
            const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[0][sq] & b.pawns_bb[0]) != 0;
            const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[1][sq] & b.pawns_bb[1]) != 0;
            if (supportedByPawn && !attackedByEnemyPawn) {
                score += OUTPOST_KNIGHT_BONUS;
            }
        }

        uint64_t bishops = b.bishops_bb[0];
        while (bishops) {
            const int sq = popLSB(bishops);
            const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[0][sq] & b.pawns_bb[0]) != 0;
            const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[1][sq] & b.pawns_bb[1]) != 0;
            if (supportedByPawn && !attackedByEnemyPawn) {
                score += (OUTPOST_BISHOP_BONUS / 2);
            }
        }
    }

    // Black side
    {
        uint64_t knights = b.knights_bb[1];
        while (knights) {
            const int sq = popLSB(knights);
            const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[1][sq] & b.pawns_bb[1]) != 0;
            const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[0][sq] & b.pawns_bb[0]) != 0;
            if (supportedByPawn && !attackedByEnemyPawn) {
                score -= OUTPOST_KNIGHT_BONUS;
            }
        }

        uint64_t bishops = b.bishops_bb[1];
        while (bishops) {
            const int sq = popLSB(bishops);
            const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[1][sq] & b.pawns_bb[1]) != 0;
            const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[0][sq] & b.pawns_bb[0]) != 0;
            if (supportedByPawn && !attackedByEnemyPawn) {
                score -= (OUTPOST_BISHOP_BONUS / 2);
            }
        }
    }

    return score;
}

int64_t Evaluator::evalMobility(const AttackData data[2]) noexcept {
    return (data[0].knightMobility + data[0].bishopMobility + data[0].rookMobility + data[0].queenMobility
          - data[1].knightMobility - data[1].bishopMobility - data[1].rookMobility - data[1].queenMobility) / 2;
}

int64_t Evaluator::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return (side == 0) ? Evaluator::evalBadBishopImpl<0>(bishops, pawns) : Evaluator::evalBadBishopImpl<1>(bishops, pawns);
}

} // namespace engine
