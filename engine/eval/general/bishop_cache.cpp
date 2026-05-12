#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalBishopPairBonusCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_BISHOP_PAIR_BONUS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    int32_t score = 0;
    if ((b.bishops_bb[0] & (b.bishops_bb[0] - 1)) != 0ULL) score += engine::BISHOP_PAIR_BONUS;
    if ((b.bishops_bb[1] & (b.bishops_bb[1] - 1)) != 0ULL) score -= engine::BISHOP_PAIR_BONUS;
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int32_t Evaluator::evalBadBishopCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_BAD_BISHOP;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score =
        evalBadBishop(b.bishops_bb[0], whitePawns, 0) +
        evalBadBishop(b.bishops_bb[1], blackPawns, 1);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

} // namespace engine
