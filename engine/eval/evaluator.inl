inline constexpr int Evaluator::manhattan(int a, int b) noexcept {
    const int fileA = chess::file(a);
    const int fileB = chess::file(b);
    const int rankA = chess::rank(a);
    const int rankB = chess::rank(b);
    return std::abs(fileA - fileB) + std::abs(rankA - rankB);
}

inline constexpr int Evaluator::chebyshev(int a, int b) noexcept {
    const int df = std::abs(chess::file(a) - chess::file(b));
    const int dr = std::abs(chess::rank(a) - chess::rank(b));
    return std::max(df, dr);
}

inline constexpr int Evaluator::edgeProximity(int sq) noexcept {
    const int r = chess::rank(sq);
    const int f = chess::file(sq);
    return 7 - std::min({r, 7 - r, f, 7 - f});
}

inline int Evaluator::ownKingProximity(uint64_t ourKingBB, int enemyKingSq) noexcept {
    if (!ourKingBB) return 0;
    return std::max(0, 14 - manhattan(std::countr_zero(ourKingBB), enemyKingSq));
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
        const int rank = chess::rank(sq);
        const int file = chess::file(sq);

        for (int r = std::max(0, rank - 2); r <= std::min(7, rank + 2); ++r) {
            for (int f = std::max(0, file - 2); f <= std::min(7, file + 2); ++f) {
                const int target = (r << 3) | f;
                const int dist = std::abs(r - rank) + std::abs(f - file);
                if (dist <= 2 && target != sq) {
                    mask |= chess::Board::BIT_MASKS[target];
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
        const int rank = chess::rank(sq);
        result[sq] = (rank > 0) ? ((chess::Board::BIT_MASKS[rank * 8]) - 1ULL) : 0ULL;
    }
    return result;
}

inline constexpr std::array<uint64_t, 64> Evaluator::initBlackForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::rank(sq);
        result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
    }
    return result;
}

inline Evaluator::PhaseInfo Evaluator::classifyPhase(const chess::Board& b) noexcept {
    PhaseInfo phase{};
    phase.phaseWeight = b.getIncrementalPhaseWeight();
    phase.totalPawns = std::popcount(b.pawns_bb[0] | b.pawns_bb[1]);
    phase.pawnOnlyEndgame = (phase.phaseWeight == 0);

    // Combined phase units in [0, 32]: weighted material (0..24, with
    // N=B=1, R=2, Q=4) plus pawn contribution (0..8, half the pawn count).
    const int32_t combined = std::min<int32_t>(32, phase.phaseWeight + (phase.totalPawns >> 1));
    // Fixed-point smoothstep: t in [0, 1024], w = 3t^2 - 2t^3.
    // combined * 32 maps 0..32 -> 0..1024.
    const int64_t t  = static_cast<int64_t>(combined) * 32;
    const int64_t t2 = t * t;
    const int64_t t3 = t2 * t;
    int64_t w = (3 * t2) / 1024 - (2 * t3) / (1024 * 1024);
    if (w < 0) w = 0;
    if (w > 1024) w = 1024;
    phase.w1024 = static_cast<int32_t>(w);
    return phase;
}

template<uint32_t Term, class Compute>
inline PhaseValue Evaluator::cachedTerm(const chess::Board& b, Compute compute) noexcept {
    if (b.hasEvalCacheTerm<Term>()) return b.getEvalCacheTerm<Term>();
    const PhaseValue score = compute();
    b.setEvalCacheTerm<Term>(score);
    return score;
}

inline void Evaluator::addKingCheckUnits(uint64_t checkers, uint64_t defenderMap,
                                         int32_t safeBonus, int32_t forcingBonus,
                                         int32_t& attackUnits) noexcept {
    attackUnits += std::popcount(checkers & ~defenderMap) * safeBonus
                 + std::popcount(checkers &  defenderMap) * forcingBonus;
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
