#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalMinorPieceDevelopmentCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_MINOR_DEVELOPMENT>(b, [&] -> PhaseValue {
        return evalMinorPieceDevelopment(b);
    });
}

PhaseValue Evaluator::evalEarlyQueenCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_EARLY_QUEEN>(b, [&] -> PhaseValue {
        return evalEarlyQueen(b);
    });
}

PhaseValue Evaluator::evalOutpostsCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_OUTPOSTS>(b, [&] -> PhaseValue {
        return evalOutposts(b);
    });
}

PhaseValue Evaluator::evalPieceCoordinationCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_PIECE_COORDINATION>(b, [&] -> PhaseValue {
        return evalPieceCoordination(b);
    });
}

} // namespace engine
