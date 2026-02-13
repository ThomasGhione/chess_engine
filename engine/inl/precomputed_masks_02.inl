namespace engine {

inline const std::array<uint64_t, 8> Engine::FILE_MASKS = []() constexpr {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        masks[f] = 0x0101010101010101ULL << f;
    }
    return masks;
}();

inline const std::array<uint64_t, 8> Engine::ADJACENT_FILES_ONLY = []() constexpr {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        uint64_t m = 0;
        if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
        if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
        masks[f] = m;
    }
    return masks;
}();

inline const std::array<uint64_t, 8> Engine::ADJACENT_AND_FILE_MASKS = []() constexpr {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        uint64_t m = (0x0101010101010101ULL << f);
        if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
        if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
        masks[f] = m;
    }
    return masks;
}();

inline const std::array<uint64_t, 64> Engine::KING_PROXIMITY_MASKS = []() constexpr {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) {
        uint64_t mask = 0;
        const int rank = chess::Board::rankOf(sq);
        const int file = chess::Board::fileOf(sq);

        for (int r = std::max(0, rank - 2); r <= std::min(7, rank + 2); ++r) {
            for (int f = std::max(0, file - 2); f <= std::min(7, file + 2); ++f) {
                const int target = (r << 3) | f;
                const int dist = std::abs(r - rank) + std::abs(f - file);
                if (dist <= 2 && target != sq) {
                    mask |= chess::Board::bitMask(target);
                }
            }
        }
        masks[sq] = mask;
    }
    return masks;
}();

} // namespace engine
