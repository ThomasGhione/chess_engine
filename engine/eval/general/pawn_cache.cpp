#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalPawnStructureCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    if (isEndgame) {
        return cachedTerm<chess::Board::EVAL_CACHE_PAWN_STRUCTURE_EG>(b, [&] {
            return evalPawnStructure(whitePawns, blackPawns, true);
        });
    }
    return cachedTerm<chess::Board::EVAL_CACHE_PAWN_STRUCTURE_MG>(b, [&] {
        return evalPawnStructure(whitePawns, blackPawns, false);
    });
}

PhaseValue Evaluator::evalPawnStructureCachedPair(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const int32_t mg = evalPawnStructureCached(b, whitePawns, blackPawns, /*isEndgame=*/false);
    const int32_t eg = evalPawnStructureCached(b, whitePawns, blackPawns, /*isEndgame=*/true);
    return PhaseValue{mg, eg};
}

int32_t Evaluator::evalCentralControlCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_CENTRAL_CONTROL>(b, [&] {
        return evalCentralControl(whitePawns, blackPawns);
    });
}

int32_t Evaluator::evalWeakSquaresCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_WEAK_SQUARES>(b, [&] {
        return evalWeakSquares(b, whitePawns, blackPawns);
    });
}

// evalBlockedPawnByBishops uses the existing board-level eval cache slot.
int32_t Evaluator::evalBlockedPawnByBishopsCached(const chess::Board& b) noexcept {
    return cachedTerm<chess::Board::EVAL_CACHE_BLOCKED_PAWN_BY_BISHOPS>(b, [&] {
        return evalBlockedPawnByBishops(b);
    });
}

} // namespace engine
