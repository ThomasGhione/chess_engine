inline Board::Board() noexcept {
    fenToBoard(STARTING_FEN);
}

inline Board::Board(const std::string& fen) {
    fenToBoard(fen);
}

inline Board::Board(const Board& other) noexcept {
    copyFromBoard(other);
}

inline Board& Board::operator=(const Board& other) noexcept {
    if (this != &other) {
        copyFromBoard(other);
    }
    return *this;
}

inline void Board::copyFromBoard(const Board& other) noexcept {
    bb = other.bb;

    chessboard = other.chessboard;
    currentHash = other.currentHash;

    historySize = other.historySize;
    if (historySize > 0) {
        std::memcpy(repetitionHistory.data(), other.repetitionHistory.data(), historySize * sizeof(uint64_t));
    }

    occupancy = other.occupancy;
    nnueAccumulator = other.nnueAccumulator;
    accPendingCount = other.accPendingCount;
    if (accPendingCount > 0) {
        std::memcpy(accPending, other.accPending,
                    static_cast<size_t>(accPendingCount) * sizeof(NNUE::AccDelta));
    }
    halfMoveClock = other.halfMoveClock;
    fullMoveClock = other.fullMoveClock;
    castle = other.castle;
    enPassant = other.enPassant;
    epHashFile = other.epHashFile;
    activeColor = other.activeColor;
    pliesFromNull = other.pliesFromNull;
}

inline void Board::rebuildBitboardsFromSquares() noexcept {
    occupancy = 0ULL;
    bb = {};
    for (uint8_t index = 0; index < 64; ++index) {
        const uint8_t piece = get(index);
        if (piece == EMPTY) continue;
        occupancy |= 1ULL << index;
        bb[piece & MASK_PIECE_TYPE][colorToIndex(piece)] |= 1ULL << index;
    }
    refreshNnueAccumulator();
}

inline bool Board::isKingSafeAfterMove(
    uint8_t movingColor,
    uint8_t fromIndex,
    uint8_t toIndex,
    uint64_t capturedMask
) const noexcept {
    const uint8_t side = colorToIndex(movingColor);
    if (!bb[KING][side]) [[unlikely]] return false;
    const uint64_t occNew =
        (occupancy & ~(1ULL << fromIndex) & ~capturedMask) | (1ULL << toIndex);
    return !attackedBy(std::countr_zero(bb[KING][side]), side ^ 1, occNew, ~capturedMask);
}

inline void Board::flushAccPending() const noexcept {
    const int n = accPendingCount;
    accPendingCount = 0;   // set first: update<> must not re-enter the queue

    // A batch is a sum of feature contributions under one basis, so rows that
    // appear on both sides cancel and what is left may be applied in any order
    // (int16 is modular, the argument updateFused already relies on). Kings
    // are features like any other as long as the basis holds: only the final
    // square of a side's king decides whether its own perspective keeps its
    // basis. A perspective whose king ends on another (bucket, flip) is left
    // dirty for the Finny refresh, which rebuilds it from the board anyway.
    // A king that is added or removed (not moved) takes the replay below.
    constexpr int CAP = 32;   // contributions per side; longer batches replay
    uint16_t sub[CAP];
    uint16_t add[CAP];
    int ns = 0;
    int na = 0;
    int kingTo[2] = {-1, -1};   // final square of each side's king, if it moved
    bool normalized = n > 0 && !nnueAccumulator.dirty[0] && !nnueAccumulator.dirty[1];
    for (int i = 0; i < n && normalized; ++i) {
        const NNUE::AccDelta& d = accPending[i];
        if (ns == CAP || na == CAP) {
            normalized = false;
            break;
        }
        if ((d.piece & MASK_PIECE_TYPE) == KING) {
            if (d.kind != NNUE::AccDelta::Move) {
                normalized = false;
                break;
            }
            kingTo[colorToIndex(d.piece)] = d.to;
        }
        const uint16_t from = static_cast<uint16_t>(d.piece << 8 | d.from);
        if (d.kind == NNUE::AccDelta::Add) {
            add[na++] = from;
        } else if (d.kind == NNUE::AccDelta::Remove) {
            sub[ns++] = from;
        } else {
            sub[ns++] = from;
            add[na++] = static_cast<uint16_t>(d.piece << 8 | d.to);
        }
    }

    if (normalized) {
        // A contribution packs (piece << 8 | square).
        const auto row = [this](int p, uint16_t c) {
            return nnueAccumulator.featureRow(p, static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c));
        };
        for (int i = 0; i < ns; ) {
            int j = 0;
            while (j < na && add[j] != sub[i]) ++j;
            if (j == na) { ++i; continue; }
            sub[i] = sub[--ns];   // swap-remove: order is irrelevant
            add[j] = add[--na];
        }
        // Perspective p is 0 for white; its own king is seen from its side.
        bool rebase[2] = {false, false};
        for (int p = 0; p < 2; ++p) {
            if (kingTo[p] < 0) continue;
            const int view = (p == 1) ? kingTo[p] : (kingTo[p] ^ 56);
            rebase[p] = NNUE::kingFeatureBase(view) != nnueAccumulator.base[p]
                     || NNUE::kingFlip(view) != nnueAccumulator.flip[p];
        }
        if (rebase[0] || rebase[1]) {
            nnueAccumulator.dirty[0] = rebase[0];
            nnueAccumulator.dirty[1] = rebase[1];
            if (rebase[0] && rebase[1]) return;
            const int q = rebase[0] ? 1 : 0;
            const int16_t* subRow[CAP];
            const int16_t* addRow[CAP];
            for (int k = 0; k < ns; ++k) subRow[k] = row(q, sub[k]);
            for (int k = 0; k < na; ++k) addRow[k] = row(q, add[k]);
            NNUE::Accumulator::applyRows(nnueAccumulator.v[q], subRow, ns, addRow, na);
            return;
        }
        NNUE::Accumulator::forEachPass(ns, na, [&]<int S, int A>(int si, int ai) {
            const int16_t* subRow[2][2];
            const int16_t* addRow[2][2];
            for (int p = 0; p < 2; ++p) {
                for (int k = 0; k < S; ++k) subRow[p][k] = row(p, sub[si + k]);
                for (int k = 0; k < A; ++k) addRow[p][k] = row(p, add[ai + k]);
            }
            nnueAccumulator.updateFused<S, A>(subRow, addRow);
        });
        return;
    }

    for (int i = 0; i < n; ++i) {
        const NNUE::AccDelta& d = accPending[i];
        switch (d.kind) {
            case NNUE::AccDelta::Add:    nnueAccumulator.update<true>(d.piece, d.from); break;
            case NNUE::AccDelta::Remove: nnueAccumulator.update<false>(d.piece, d.from); break;
            default:                     nnueAccumulator.updateMove(d.piece, d.from, d.to); break;
        }
    }
}

// From-scratch accumulator rebuild. rebuildBitboardsFromSquares sets the
// bitboards directly (NOT through addPieceToBB), so bulk rebuilds never
// double-count: they land here once at the end instead.
inline void Board::refreshNnueAccumulator() noexcept {
    if (NNUE::activeNetwork == nullptr) return;
    accPendingCount = 0;   // rebuilt from the squares below; queued work is moot
    // No kings (unit-test fragments, mid-load states): leave the accumulator
    // as-is; evaluation is guarded upstream by the kings-missing check.
    if (bb[KING][0] == 0 || bb[KING][1] == 0) return;
    const int wKingLerf      = std::countr_zero(bb[KING][0]) ^ 56;
    const int bKingLerfView  = std::countr_zero(bb[KING][1]); // lerf ^ 56
    nnueAccumulator.resetWithKings(wKingLerf, bKingLerfView);
    for (uint8_t index = 0; index < 64; ++index) {
        const uint8_t piece = get(index);
        if (piece != EMPTY) nnueAccumulator.update<true>(piece, index);
    }
}

// HalfKA lazy refresh: rebuild only the perspectives whose own king crossed
// bucket/flip since the last clean state, as a Finny-table diff against the
// cached accumulator for the target (bucket, flip). Called from
// consistent-board points only (NNUE::evaluate, selftest) - never mid-doMove.
inline void Board::ensureNnueAccumulatorClean() const noexcept {
    if (NNUE::activeNetwork == nullptr) return;
    flushAccPending();
    if (!(nnueAccumulator.dirty[0] || nnueAccumulator.dirty[1])) [[likely]] return;
    if (bb[KING][0] == 0 || bb[KING][1] == 0) return;

    // Per-thread cache (Lazy SMP: each helper searches its own Board on its
    // own thread). Stale entries from other positions are still correct diff
    // bases - no invalidation needed, ever.
    static thread_local NNUE::FinnyTable finny;
    finny.ensureInitialised();

    for (int p = 0; p < 2; ++p) {
        if (!nnueAccumulator.dirty[p]) continue;
        // Own king square from this perspective's view: lerf for white,
        // lerf ^ 56 for black - which folds back to the raw engine index.
        const int engineKing = std::countr_zero(bb[KING][p]);
        const int ownKingView = (p == 1) ? engineKing : (engineKing ^ 56);
        const int rowBase = NNUE::kingFeatureBase(ownKingView);
        const int rowFlip = NNUE::kingFlip(ownKingView);
        NNUE::FinnyEntry& e =
            finny.entry[p][NNUE::KING_BUCKET_MAP[ownKingView]][rowFlip ? 1 : 0];

        // Collect the diff first and apply it in chunks: a board holds at most
        // 64 pieces, so neither side of the diff can exceed that.
        const int16_t* sub[64];
        const int16_t* add[64];
        int ns = 0;
        int na = 0;
        for (int c = 0; c < 2; ++c) {
            for (int t = 0; t < 6; ++t) {
                const uint8_t piece = static_cast<uint8_t>((c == 0 ? 0x8 : 0) | (t + 1));
                uint64_t added   = bb[t + 1][c] & ~e.bb[c][t];
                uint64_t removed = e.bb[c][t] & ~bb[t + 1][c];
                while (added)
                    add[na++] = NNUE::Accumulator::rowFor(rowBase, rowFlip, p, piece,
                                                          static_cast<uint8_t>(pieces::popLSB(added)));
                while (removed)
                    sub[ns++] = NNUE::Accumulator::rowFor(rowBase, rowFlip, p, piece,
                                                          static_cast<uint8_t>(pieces::popLSB(removed)));
                e.bb[c][t] = bb[t + 1][c];
            }
        }
        NNUE::Accumulator::applyRows(e.v, sub, ns, add, na);

        std::memcpy(nnueAccumulator.v[p], e.v, sizeof(e.v));
        nnueAccumulator.base[p] = rowBase;
        nnueAccumulator.flip[p] = static_cast<uint8_t>(rowFlip);
        nnueAccumulator.dirty[p] = false;
    }
}