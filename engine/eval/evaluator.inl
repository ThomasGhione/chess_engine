inline constexpr int Evaluator::manhattan(int a, int b) noexcept {
    return std::abs((a & 7) - (b & 7)) + std::abs((a >> 3) - (b >> 3));
}

inline constexpr uint64_t Evaluator::adjacentFilesMask(int file) noexcept {
    uint64_t m = 0;
    if (file > 0) m |= chess::Board::fileMask(file - 1);
    if (file < 7) m |= chess::Board::fileMask(file + 1);
    return m;
}

inline constexpr std::array<uint64_t, 8> Evaluator::initFileMasks() noexcept {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        masks[f] = 0x0101010101010101ULL << f;
    }
    return masks;
}

inline constexpr std::array<uint64_t, 8> Evaluator::initAdjacentFilesOnly() noexcept {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        uint64_t m = 0;
        if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
        if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
        masks[f] = m;
    }
    return masks;
}

inline constexpr std::array<uint64_t, 8> Evaluator::initAdjacentAndFileMasks() noexcept {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        uint64_t m = (0x0101010101010101ULL << f);
        if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
        if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
        masks[f] = m;
    }
    return masks;
}

inline constexpr std::array<uint64_t, 64> Evaluator::initKingProximityMasks() noexcept {
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
}

inline constexpr std::array<uint64_t, 64> Evaluator::initWhiteForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::Board::rankOf(sq);
        result[sq] = (rank > 0) ? ((chess::Board::bitMask(rank * 8)) - 1ULL) : 0ULL;
    }
    return result;
}

inline constexpr std::array<uint64_t, 64> Evaluator::initBlackForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::Board::rankOf(sq);
        result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
    }
    return result;
}

template<bool IsEndgame>
inline constexpr int64_t Evaluator::evalInitiativeImpl(uint8_t activeColor) noexcept {
    constexpr int64_t bonus = IsEndgame ? INIT_BONUS_EG : INIT_BONUS_MG;
    return (activeColor == chess::Board::WHITE) ? bonus : -bonus;
}

template<int Side>
inline constexpr int64_t Evaluator::evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
    static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");

    const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
    const int lightPawnCount = __builtin_popcountll(pawns & LIGHT_SQUARES);

    const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
    const int lightBishops = __builtin_popcountll(bishops & LIGHT_SQUARES);

    const int64_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * 8);

    if constexpr (Side == 0) {
        return score;
    } else {
        return -score;
    }
}

inline constexpr int64_t Evaluator::getPieceValue(uint8_t pieceType) noexcept {
    return PIECE_VALUES[pieceType & chess::Board::MASK_PIECE_TYPE];
}

inline void Evaluator::ensureAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
    if (!data[0].isComputed) {
        computeAttackData(data, b, occ);
    }
}

inline uint8_t Evaluator::popLSB(uint64_t& bb) noexcept{
    const uint8_t index = static_cast<uint8_t>(__builtin_ctzll(bb));
    bb &= (bb - 1);
    return index;
}
