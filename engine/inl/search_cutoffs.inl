namespace engine {

inline bool Engine::isBetaCutoff(int64_t score, int64_t alpha, int64_t beta, bool isWhite) noexcept {
    return isWhite ? (score >= beta) : (score <= alpha);
}

inline void Engine::updateBound(int64_t score, int64_t& alpha, int64_t& beta, bool isWhite) noexcept {
    if (isWhite) {
        if (score > alpha) alpha = score;
    } else {
        if (score < beta) beta = score;
    }
}

inline bool Engine::shouldDeltaPrune(int64_t standPat, int64_t margin, int64_t alpha, int64_t beta, bool isWhite) noexcept {
    return isWhite ? (standPat + margin < alpha) : (standPat - margin > beta);
}

inline int64_t Engine::cutoffValue(int64_t alpha, int64_t beta, bool isWhite) noexcept {
    return isWhite ? beta : alpha;
}

inline bool Engine::shouldResearchPVS(int64_t score, int64_t alphaBound, int64_t betaBound, bool isWhite) noexcept {
    return isWhite ? (score > alphaBound) : (score < betaBound);
}

inline constexpr int64_t Engine::getPieceValue(uint8_t pieceType) noexcept {
    return PIECE_VALUES[pieceType & chess::Board::MASK_PIECE_TYPE];
}

} // namespace engine
