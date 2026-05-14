#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalPawnStructureCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    if (isEndgame) {
        constexpr uint32_t TERM = chess::Board::EVAL_CACHE_PAWN_STRUCTURE_EG;
        if (b.hasEvalCacheTerm<TERM>()) {
            return b.getEvalCacheTerm<TERM>();
        }
        const int32_t score = evalPawnStructure(whitePawns, blackPawns, true);
        b.setEvalCacheTerm<TERM>(score);
        return score;
    }

    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_PAWN_STRUCTURE_MG;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalPawnStructure(whitePawns, blackPawns, false);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int32_t Evaluator::evalCentralControlCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_CENTRAL_CONTROL;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalCentralControl(whitePawns, blackPawns);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int32_t Evaluator::evalWeakSquaresCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_WEAK_SQUARES;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalWeakSquares(b, whitePawns, blackPawns);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

// evalBlockedPawnByBishops uses the existing board-level eval cache slot.
int32_t Evaluator::evalBlockedPawnByBishopsCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_BLOCKED_PAWN_BY_BISHOPS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalBlockedPawnByBishops(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

} // namespace engine
