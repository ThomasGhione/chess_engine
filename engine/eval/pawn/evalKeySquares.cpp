#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalPassedPawnKeySquares(const chess::Board& b,
                                                uint64_t whitePawns,
                                                uint64_t blackPawns) noexcept {
    if (!whitePawns && !blackPawns) return {};

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

        int32_t score = 0;
        uint64_t pawns = ownPawns;
        while (pawns) {
            const int sq = std::countr_zero(pawns);
            pawns &= pawns - 1;

            const int file = chess::file(sq);
            const int rank = chess::rank(sq);

            if ((enemyPawns & ADJACENT_AND_FILE_MASKS[file] & fwd[sq]) != 0ULL) continue;

            const int step1 = isWhite ? rank - 1 : rank + 1;
            const int step2 = isWhite ? rank - 2 : rank + 2;

            uint64_t keySqs = 0ULL;
            for (int step : {step1, step2}) {
                if (step < 0 || step > 7) continue;
                for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); ++f) {
                    keySqs |= chess::Board::BIT_MASKS[step * 8 + f];
                }
            }

            constexpr int32_t OUR_KING_ON_KEY   = 18;
            constexpr int32_t ENEMY_KING_ON_KEY  = 14;

            if (keySqs & ourKingBB)   score += sign * OUR_KING_ON_KEY;
            if (keySqs & enemyKingBB) score -= sign * ENEMY_KING_ON_KEY;

            const int keyRank = isWhite ? std::max(0, rank - 2) : std::min(7, rank + 2);
            const int keyCenterSq = keyRank * 8 + file;

            const int ourDist   = manhattan(ourKingSq,   keyCenterSq);
            const int enemyDist = manhattan(enemyKingSq, keyCenterSq);

            constexpr int32_t PROXIMITY_SCALE = 3;
            score += sign * (enemyDist - ourDist) * PROXIMITY_SCALE;
        }

        return score;
    };

    const int32_t total = evalSide(whitePawns, blackPawns, true)
                        + evalSide(blackPawns, whitePawns, false);
    return {0, total};
}

} // namespace engine
