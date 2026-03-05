namespace engine {

inline void Engine::addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int32_t* table, int32_t& eval) noexcept {
    while (bbWhite) {
        const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbWhite));
        bbWhite &= (bbWhite - 1);
        eval += table[sq];
    }
    while (bbBlack) {
        const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbBlack));
        bbBlack &= (bbBlack - 1);
        const uint8_t idx = mirrorIndex(sq);
        eval -= table[idx];
    }
}

template<bool IsEndgame>
inline constexpr int32_t Engine::evalInitiativeImpl(uint8_t activeColor) noexcept {
    constexpr int32_t bonus = IsEndgame ? INIT_BONUS_EG : INIT_BONUS_MG;
    return (activeColor == chess::Board::WHITE) ? bonus : -bonus;
}

inline int32_t Engine::evalInitiative(const chess::Board& b, bool isEndgame) noexcept {
    return isEndgame
        ? evalInitiativeImpl<true>(b.getActiveColor())
        : evalInitiativeImpl<false>(b.getActiveColor());
}

template<int Side>
inline constexpr int32_t Engine::evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
    static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");

    const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
    const int lightPawnCount = __builtin_popcountll(pawns & LIGHT_SQUARES);

    const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
    const int lightBishops = __builtin_popcountll(bishops & LIGHT_SQUARES);

    const int32_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * 8);

    if constexpr (Side == 0) {
        return score;
    } else {
        return -score;
    }
}

} // namespace engine
