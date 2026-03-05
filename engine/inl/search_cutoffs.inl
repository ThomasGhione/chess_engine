namespace engine {

inline bool Engine::isBetaCutoff(int32_t score, int32_t alpha, int32_t beta, bool isWhite) noexcept {
    return isWhite ? (score >= beta) : (score <= alpha);
}

inline void Engine::updateBound(int32_t score, int32_t& alpha, int32_t& beta, bool isWhite) noexcept {
    if (isWhite) {
        if (score > alpha) alpha = score;
    } else {
        if (score < beta) beta = score;
    }
}

inline bool Engine::shouldDeltaPrune(int32_t standPat, int32_t margin, int32_t alpha, int32_t beta, bool isWhite) noexcept {
    const int64_t standPat64 = static_cast<int64_t>(standPat);
    const int64_t margin64 = static_cast<int64_t>(margin);
    const int64_t alpha64 = static_cast<int64_t>(alpha);
    const int64_t beta64 = static_cast<int64_t>(beta);
    return isWhite ? (standPat64 + margin64 < alpha64) : (standPat64 - margin64 > beta64);
}

inline int32_t Engine::cutoffValue(int32_t alpha, int32_t beta, bool isWhite) noexcept {
    return isWhite ? beta : alpha;
}

inline bool Engine::shouldResearchPVS(int32_t score, int32_t alphaBound, int32_t betaBound, bool isWhite) noexcept {
    return isWhite ? (score > alphaBound) : (score < betaBound);
}

} // namespace engine
