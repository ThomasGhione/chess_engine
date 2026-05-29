#include "../evaluator.hpp"

namespace engine {

PhaseValue Evaluator::evalPawnStructureCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, bool /*isEndgame*/) noexcept {
    // PhaseValue cached: a single slot covers both mg and eg.
    return cachedTerm<chess::Board::EVAL_CACHE_PAWN_STRUCTURE_MG>(b, [&] -> PhaseValue {
        return evalPawnStructure(whitePawns, blackPawns, false);
    });
}

PhaseValue Evaluator::evalPawnStructureCachedPair(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return evalPawnStructureCached(b, whitePawns, blackPawns, false);
}

PhaseValue Evaluator::evalCentralControlCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_CENTRAL_CONTROL>(b, [&] -> PhaseValue {
        return evalCentralControl(whitePawns, blackPawns);
    });
}

PhaseValue Evaluator::evalWeakSquaresCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_WEAK_SQUARES>(b, [&] -> PhaseValue {
        return evalWeakSquares(b, whitePawns, blackPawns);
    });
}

PhaseValue Evaluator::evalBlockedPawnByBishopsCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_BLOCKED_PAWN_BY_BISHOPS>(b, [&] -> PhaseValue {
        return evalBlockedPawnByBishops(b);
    });
}

} // namespace engine
