namespace engine {

template<bool IsWhite>
inline constexpr int64_t Engine::initialBest() noexcept {
    return IsWhite ? NEG_INF : POS_INF;
}

inline constexpr int64_t Engine::initialBest(bool isWhite) noexcept {
    return isWhite ? NEG_INF : POS_INF;
}

template<bool IsWhite>
inline constexpr bool Engine::isBetter(int64_t newScore, int64_t currentBest) noexcept {
    return IsWhite ? (newScore > currentBest) : (newScore < currentBest);
}

inline constexpr bool Engine::isBetter(int64_t newScore, int64_t currentBest, bool isWhite) noexcept {
    return isWhite ? (newScore > currentBest) : (newScore < currentBest);
}

} // namespace engine
