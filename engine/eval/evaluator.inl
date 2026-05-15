inline constexpr int Evaluator::manhattan(int a, int b) noexcept {
    const int fileA = chess::Board::file(a);
    const int fileB = chess::Board::file(b);
    const int rankA = chess::Board::rank(a);
    const int rankB = chess::Board::rank(b);
    return std::abs(fileA - fileB) + std::abs(rankA - rankB);
}

inline constexpr int Evaluator::chebyshev(int a, int b) noexcept {
    const int df = std::abs(chess::Board::file(a) - chess::Board::file(b));
    const int dr = std::abs(chess::Board::rank(a) - chess::Board::rank(b));
    return std::max(df, dr);
}

inline constexpr int Evaluator::edgeProximity(int sq) noexcept {
    const int r = chess::Board::rank(sq);
    const int f = chess::Board::file(sq);
    return 7 - std::min({r, 7 - r, f, 7 - f});
}

inline int Evaluator::ownKingProximity(uint64_t ourKingBB, int enemyKingSq) noexcept {
    if (!ourKingBB) return 0;
    return std::max(0, 14 - manhattan(__builtin_ctzll(ourKingBB), enemyKingSq));
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
        const int rank = chess::Board::rank(sq);
        const int file = chess::Board::file(sq);

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
        const int rank = chess::Board::rank(sq);
        result[sq] = (rank > 0) ? ((chess::Board::bitMask(rank * 8)) - 1ULL) : 0ULL;
    }
    return result;
}

inline constexpr std::array<uint64_t, 64> Evaluator::initBlackForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::Board::rank(sq);
        result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
    }
    return result;
}

template<bool IsEndgame>
inline int32_t Evaluator::evalInitiativeImpl(uint8_t activeColor) noexcept {
    const int32_t bonus = IsEndgame ? engine::INIT_BONUS_EG : engine::INIT_BONUS_MG;
    return (activeColor == chess::Board::WHITE) ? bonus : -bonus;
}


inline Evaluator::PhaseInfo Evaluator::classifyPhase(const chess::Board& b) noexcept {
    PhaseInfo phase{};
    phase.fullMoves = b.getFullMoveClock();
    phase.nonPawnMajors = b.getIncrementalNonPawnMajorCount();
    phase.isEndgame = (phase.nonPawnMajors <= PIECE_ENDGAME_THRESHOLD);
    phase.isOpening = !phase.isEndgame && (phase.fullMoves < OPENING_MOVES);
    phase.isEarlyMiddlegame = !phase.isEndgame && !phase.isOpening && (phase.fullMoves < EARLY_MG_MOVES);
    return phase;
}

template<uint32_t Term, class Compute>
inline int32_t Evaluator::cachedTerm(const chess::Board& b, Compute compute) noexcept {
    if (b.hasEvalCacheTerm<Term>()) return b.getEvalCacheTerm<Term>();
    const int32_t score = compute();
    b.setEvalCacheTerm<Term>(score);
    return score;
}

inline void Evaluator::addKingCheckUnits(uint64_t checkers, uint64_t defenderMap,
                                         int32_t safeBonus, int32_t forcingBonus,
                                         int32_t& attackUnits) noexcept {
    attackUnits += __builtin_popcountll(checkers & ~defenderMap) * safeBonus
                 + __builtin_popcountll(checkers &  defenderMap) * forcingBonus;
}

inline uint64_t Evaluator::knightAttacksLookup(uint8_t sq, uint64_t) noexcept {
    return pieces::KNIGHT_ATTACKS[sq];
}

template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
inline void Evaluator::accumulateKingZoneAttackers(uint64_t piecesBb, uint64_t kingZone, uint64_t occ,
                                                   int32_t weight, int& attackerCount, int32_t& attackWeight) noexcept {
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        if (AttackFn(sq, occ) & kingZone) {
            ++attackerCount;
            attackWeight += weight;
        }
    }
}
