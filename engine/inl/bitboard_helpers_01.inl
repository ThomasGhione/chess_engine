namespace engine {

inline uint8_t popLSB(uint64_t& bb) noexcept {
    const uint8_t index = static_cast<uint8_t>(__builtin_ctzll(bb));
    bb &= (bb - 1);
    return index;
}

inline constexpr int Engine::manhattan(int a, int b) noexcept {
    return std::abs((a & 7) - (b & 7)) + std::abs((a >> 3) - (b >> 3));
}

inline constexpr uint64_t Engine::adjacentFilesMask(int file) noexcept {
    uint64_t m = 0;
    if (file > 0) m |= chess::Board::fileMask(file - 1);
    if (file < 7) m |= chess::Board::fileMask(file + 1);
    return m;
}

inline constexpr std::array<uint64_t, 64> Engine::initWhiteForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::Board::rankOf(sq);
        result[sq] = (rank > 0) ? ((chess::Board::bitMask(rank * 8)) - 1ULL) : 0ULL;
    }
    return result;
}

inline constexpr std::array<uint64_t, 64> Engine::initBlackForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::Board::rankOf(sq);
        result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
    }
    return result;
}

} // namespace engine
