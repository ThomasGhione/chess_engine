#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalRuleOfSquare(const chess::Board& b,
                                       uint64_t whitePawns,
                                       uint64_t blackPawns) noexcept {
    if (!whitePawns && !blackPawns) return {};

    auto kingInSquare = [](int kingSq, int pawnSq, bool isWhite,
                           bool enemyToMove) -> bool {
        const int pawnRank   = chess::rank(pawnSq);
        const int pawnFile   = chess::file(pawnSq);
        const int movesToProm = isWhite ? pawnRank : (7 - pawnRank);
        if (movesToProm <= 0) return true;

        const int promRank = isWhite ? 0 : 7;
        const int dist = std::max(
            std::abs(chess::rank(kingSq) - promRank),
            std::abs(chess::file(kingSq) - pawnFile)
        );
        return dist <= movesToProm + (enemyToMove ? 1 : 0);
    };

    auto evalSide = [&](uint64_t ownPawns, uint64_t enemyPawns,
                        bool isWhite) -> int32_t {
        const int side    = isWhite ? 0 : 1;
        const int oppSide = side ^ 1;
        const int sign    = isWhite ? 1 : -1;

        const uint64_t ourKingBB   = b.kings_bb[side];
        const uint64_t enemyKingBB = b.kings_bb[oppSide];
        if (!ourKingBB || !enemyKingBB) return 0;

        const int ourKingSq   = std::countr_zero(ourKingBB);
        const int enemyKingSq = std::countr_zero(enemyKingBB);
        const auto& fwd = isWhite ? WHITE_FORWARD_FILL : BLACK_FORWARD_FILL;

        const bool enemyToMove = (b.getActiveColor() != (isWhite ? chess::Board::WHITE
                                                                  : chess::Board::BLACK));

        int32_t score = 0;
        uint64_t pawns = ownPawns;
        while (pawns) {
            const int sq = std::countr_zero(pawns);
            pawns &= pawns - 1;

            const int file = chess::file(sq);

            if ((enemyPawns & ADJACENT_AND_FILE_MASKS[file] & fwd[sq]) != 0ULL) continue;

            if (!kingInSquare(enemyKingSq, sq, isWhite, enemyToMove)) {
                constexpr int32_t FREE_PROMO_BONUS = 90;
                score += sign * FREE_PROMO_BONUS;
                continue;
            }

            const int pawnRank    = chess::rank(sq);
            const int movesToProm = isWhite ? pawnRank : (7 - pawnRank);
            const int promRank    = isWhite ? 0 : 7;
            const int distEnemy   = std::max(
                std::abs(chess::rank(enemyKingSq) - promRank),
                std::abs(chess::file(enemyKingSq) - file)
            );
            const int margin = (movesToProm + (enemyToMove ? 1 : 0)) - distEnemy;
            if (margin <= 1) {
                const int distOur = std::max(
                    std::abs(chess::rank(ourKingSq) - promRank),
                    std::abs(chess::file(ourKingSq) - file)
                );
                constexpr int32_t TIGHT_RACE_KING_BONUS = 6;
                score += sign * (14 - std::min(14, distOur)) * TIGHT_RACE_KING_BONUS / 14;
            }
        }
        return score;
    };

    const int32_t total = evalSide(whitePawns, blackPawns, true)
                        + evalSide(blackPawns, whitePawns, false);
    // EG-only feature.
    return {0, total};
}

} // namespace engine
