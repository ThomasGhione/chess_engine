#include "../evaluator.hpp"

namespace engine {

// Rule of the square.
//
// A passed pawn promotes freely if the enemy king cannot enter its "square"
// (Chebyshev distance to the promotion square ≤ moves left to promote).
// Gives a large bonus when the pawn is winning the race and a partial bonus
// when the race is very tight (encourages king support).
//
// Active in endgame and pawn-only endgame phases.

int32_t Evaluator::evalRuleOfSquare(const chess::Board& b,
                                    uint64_t whitePawns,
                                    uint64_t blackPawns) noexcept {
    if (!whitePawns && !blackPawns) return 0;

    // Returns true if kingSq can enter the square of a passer on pawnSq.
    // enemyToMove: enemy king gets an extra tempo.
    auto kingInSquare = [](int kingSq, int pawnSq, bool isWhite,
                           bool enemyToMove) -> bool {
        const int pawnRank   = chess::Board::rank(pawnSq);
        const int pawnFile   = chess::Board::file(pawnSq);
        const int movesToProm = isWhite ? pawnRank : (7 - pawnRank);
        if (movesToProm <= 0) return true;

        const int promRank = isWhite ? 0 : 7;
        const int dist = std::max(
            std::abs(chess::Board::rank(kingSq) - promRank),
            std::abs(chess::Board::file(kingSq) - pawnFile)
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

        const int ourKingSq   = __builtin_ctzll(ourKingBB);
        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const auto& fwd = isWhite ? WHITE_FORWARD_FILL : BLACK_FORWARD_FILL;

        // Enemy to move relative to the pawn's advancing side.
        const bool enemyToMove = (b.getActiveColor() != (isWhite ? chess::Board::WHITE
                                                                  : chess::Board::BLACK));

        int32_t score = 0;
        uint64_t pawns = ownPawns;
        while (pawns) {
            const int sq = __builtin_ctzll(pawns);
            pawns &= pawns - 1;

            const int file = chess::Board::file(sq);

            // Only passed pawns.
            if ((enemyPawns & ADJACENT_AND_FILE_MASKS[file] & fwd[sq]) != 0ULL) continue;

            if (!kingInSquare(enemyKingSq, sq, isWhite, enemyToMove)) {
                // Pawn is outside the enemy king's square → free promotion.
                constexpr int32_t FREE_PROMO_BONUS = 90;
                score += sign * FREE_PROMO_BONUS;
                continue;
            }

            // Tight race: enemy is just barely inside the square.
            // Reward our king being close to support the pawn.
            const int pawnRank    = chess::Board::rank(sq);
            const int movesToProm = isWhite ? pawnRank : (7 - pawnRank);
            const int promRank    = isWhite ? 0 : 7;
            const int distEnemy   = std::max(
                std::abs(chess::Board::rank(enemyKingSq) - promRank),
                std::abs(chess::Board::file(enemyKingSq) - file)
            );
            const int margin = (movesToProm + (enemyToMove ? 1 : 0)) - distEnemy;
            if (margin <= 1) {
                const int distOur = std::max(
                    std::abs(chess::Board::rank(ourKingSq) - promRank),
                    std::abs(chess::Board::file(ourKingSq) - file)
                );
                constexpr int32_t TIGHT_RACE_KING_BONUS = 6;
                score += sign * (14 - std::min(14, distOur)) * TIGHT_RACE_KING_BONUS / 14;
            }
        }
        return score;
    };

    return evalSide(whitePawns, blackPawns, true)
         + evalSide(blackPawns, whitePawns, false);
}

} // namespace engine
