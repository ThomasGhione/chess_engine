#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalBishopPairBonusCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_BISHOP_PAIR_BONUS>(b, [&] {
        int32_t score = 0;
        if ((b.bishops_bb[0] & (b.bishops_bb[0] - 1)) != 0ULL) score += engine::BISHOP_PAIR_BONUS;
        if ((b.bishops_bb[1] & (b.bishops_bb[1] - 1)) != 0ULL) score -= engine::BISHOP_PAIR_BONUS;
        return score;
    });
}

int32_t Evaluator::evalBadBishopCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_BAD_BISHOP>(b, [&] {
        return evalBadBishop(b.bishops_bb[0], whitePawns, 0) +
               evalBadBishop(b.bishops_bb[1], blackPawns, 1);
    });
}

int32_t Evaluator::evalBishopVsKnightCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_BISHOP_VS_KNIGHT>(b, [&] {
        return evalBishopVsKnight(b, whitePawns, blackPawns);
    });
}

} // namespace engine
