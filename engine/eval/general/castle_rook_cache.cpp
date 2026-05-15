#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalCastlingBonusCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_CASTLING_BONUS>(b, [&] {
        return evalCastlingBonus(b);
    });
}

int32_t Evaluator::evalRooksCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_ROOKS>(b, [&] {
        return evalRooks(b.rooks_bb[0], b.rooks_bb[1], whitePawns, blackPawns);
    });
}

} // namespace engine
