#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalMinorPieceDevelopmentCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_MINOR_DEVELOPMENT>(b, [&] {
        return evalMinorPieceDevelopment(b);
    });
}

int32_t Evaluator::evalEarlyQueenCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_EARLY_QUEEN>(b, [&] {
        return evalEarlyQueen(b);
    });
}

int32_t Evaluator::evalOutpostsCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_OUTPOSTS>(b, [&] {
        return evalOutposts(b);
    });
}

int32_t Evaluator::evalPieceCoordinationCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_PIECE_COORDINATION>(b, [&] {
        return evalPieceCoordination(b);
    });
}

} // namespace engine
