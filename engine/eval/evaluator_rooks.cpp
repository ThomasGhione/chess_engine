#include "evaluator.hpp"
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
    const int64_t fileBonus = (!ownPawnOnFile) * ((!oppPawnOnFile) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS) * sign;
    score += fileBonus + (rank == targetRank) * (sign * engine::ROOK_ON_SEVENTH_BONUS);

    if (!rooks) {
      return score;
    }

    const int sq2 = popLSB(rooks);
    const int file2 = sq2 & 7;
    const int rank2 = sq2 >> 3;
    const uint64_t fm2 = FILE_MASKS[file2];
    const bool ownPawnOnFile2 = (ownPawns & fm2) != 0;
    const bool oppPawnOnFile2 = (oppPawns & fm2) != 0;
    const int64_t fileBonus2 = (!ownPawnOnFile2) * ((!oppPawnOnFile2) ? engine::OPEN_FILE_ROOK_BONUS : engine::SEMI_OPEN_FILE_ROOK_BONUS) * sign;
    score += fileBonus2 + (rank2 == targetRank) * (sign * engine::ROOK_ON_SEVENTH_BONUS);

    return score;
}

int64_t Evaluator::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const int64_t whiteRookScore = Evaluator::evalRooksForColor(0, whiteRooks, whitePawns, blackPawns);
    const int64_t blackRookScore = Evaluator::evalRooksForColor(1, blackRooks, blackPawns, whitePawns);

    return whiteRookScore + blackRookScore;
}

} // namespace engine
