#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalCastlingBonusCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_CASTLING_BONUS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalCastlingBonus(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int32_t Evaluator::evalRooksCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_ROOKS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalRooks(b.rooks_bb[0], b.rooks_bb[1], whitePawns, blackPawns);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

} // namespace engine
