#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalMinorPieceDevelopmentCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_MINOR_DEVELOPMENT;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalMinorPieceDevelopment(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int32_t Evaluator::evalEarlyQueenCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_EARLY_QUEEN;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalEarlyQueen(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int32_t Evaluator::evalOutpostsCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_OUTPOSTS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalOutposts(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int32_t Evaluator::evalPieceCoordinationCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_PIECE_COORDINATION;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int32_t score = evalPieceCoordination(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

} // namespace engine
