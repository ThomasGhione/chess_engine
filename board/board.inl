// ==============================
// Constructors
// ==============================
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
    pawns_bb = other.pawns_bb;
    knights_bb = other.knights_bb;
    bishops_bb = other.bishops_bb;
    rooks_bb = other.rooks_bb;
    queens_bb = other.queens_bb;
    kings_bb = other.kings_bb;

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

// ==============================
// Public Board API
// ==============================
__attribute__((hot, always_inline))
inline void Board::set(uint8_t index, piece_id value) noexcept {
    chessboard[index] = static_cast<uint8_t>(value & MASK_PIECE);
}


// ==============================
// Board Internals
// ==============================

inline void Board::rebuildBitboardsFromSquares() noexcept {
    // Reset all bitboards
    occupancy       = 0ULL;
    pawns_bb[0]     = pawns_bb[1]     = 0ULL;
    knights_bb[0]   = knights_bb[1]   = 0ULL;
    bishops_bb[0]   = bishops_bb[1]   = 0ULL;
    rooks_bb[0]     = rooks_bb[1]     = 0ULL;
    queens_bb[0]    = queens_bb[1]    = 0ULL;
    kings_bb[0]     = kings_bb[1]     = 0ULL;

    // Single loop: iterate all 64 squares directly
    // index = rank * 8 + file, where rank 0 = row 8, rank 7 = row 1
    for (uint8_t index = 0; index < 64; ++index) {
        const uint8_t piece = get(index);
        
        if (piece == EMPTY) continue;
        
        const uint64_t bit = BIT_MASKS[index];
        const uint8_t color = colorToIndex(piece);

        occupancy |= bit;
        dispatchPieceBBUpdate<true>(piece & MASK_PIECE_TYPE, color, bit);
    }

    refreshNnueAccumulator();
}

__attribute__((always_inline))
inline void Board::fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept {
    occupancy |= BIT_MASKS[toIndex];  // Set the bit at 'to' position    
    occupancy &= ~BIT_MASKS[fromIndex]; // Clear the bit at 'from' position
}

inline bool Board::isKingSafeAfterMove(
    uint8_t movingColor,
    uint8_t fromIndex,
    uint8_t toIndex,
    uint64_t capturedMask
) const noexcept {
    const uint8_t side = colorToIndex(movingColor);
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return false;

    const uint8_t oppSide = side ^ 1;
    const uint8_t kingSq = std::countr_zero(kingBB);

    const uint64_t occNew =
        (occupancy & ~BIT_MASKS[fromIndex] & ~capturedMask) | BIT_MASKS[toIndex];
    const uint64_t keep = ~capturedMask;

    return !isKingAttackedCustom(kingSq, oppSide, occNew,
                                 pawns_bb[oppSide]   & keep,
                                 knights_bb[oppSide] & keep,
                                 bishops_bb[oppSide] & keep,
                                 rooks_bb[oppSide]   & keep,
                                 queens_bb[oppSide]  & keep,
                                 kings_bb[oppSide]   & keep);
}

template<uint8_t PieceType, bool Add>
inline void Board::updatePieceTypeBB(uint8_t color, uint64_t bit) noexcept {
    if constexpr (PieceType == PAWN) {
        if constexpr (Add) pawns_bb[color] |= bit;
        else pawns_bb[color] &= ~bit;
    } else if constexpr (PieceType == KNIGHT) {
        if constexpr (Add) knights_bb[color] |= bit;
        else knights_bb[color] &= ~bit;
    } else if constexpr (PieceType == BISHOP) {
        if constexpr (Add) bishops_bb[color] |= bit;
        else bishops_bb[color] &= ~bit;
    } else if constexpr (PieceType == ROOK) {
        if constexpr (Add) rooks_bb[color] |= bit;
        else rooks_bb[color] &= ~bit;
    } else if constexpr (PieceType == QUEEN) {
        if constexpr (Add) queens_bb[color] |= bit;
        else queens_bb[color] &= ~bit;
    } else if constexpr (PieceType == KING) {
        if constexpr (Add) kings_bb[color] |= bit;
        else kings_bb[color] &= ~bit;
    }
}

template<bool Add>
inline void Board::dispatchPieceBBUpdate(uint8_t pieceType, uint8_t color, uint64_t bit) noexcept {
    switch (pieceType) {
        case PAWN:   updatePieceTypeBB<PAWN, Add>(color, bit); break;
        case KNIGHT: updatePieceTypeBB<KNIGHT, Add>(color, bit); break;
        case BISHOP: updatePieceTypeBB<BISHOP, Add>(color, bit); break;
        case ROOK:   updatePieceTypeBB<ROOK, Add>(color, bit); break;
        case QUEEN:  updatePieceTypeBB<QUEEN, Add>(color, bit); break;
        case KING:   updatePieceTypeBB<KING, Add>(color, bit); break;
        default: break;
    }
}

// The queue cancels a delta against the pending one it inverts. undoMove emits
// exactly the inverse operations in reverse order, so a do/undo pair that never
// reached an evaluate annihilates here and never touches a row.
inline void Board::queueAccAdd(uint8_t piece, uint8_t index) const noexcept {
    if (accPendingCount > 0) {
        const NNUE::AccDelta& d = accPending[accPendingCount - 1];
        if (d.kind == NNUE::AccDelta::Remove && d.piece == piece && d.from == index) {
            --accPendingCount;
            return;
        }
    }
    if (accPendingCount == NNUE::MAX_ACC_PENDING) [[unlikely]] flushAccPending();
    accPending[accPendingCount++] = {NNUE::AccDelta::Add, piece, index, 0};
}

inline void Board::queueAccRemove(uint8_t piece, uint8_t index) const noexcept {
    if (accPendingCount > 0) {
        const NNUE::AccDelta& d = accPending[accPendingCount - 1];
        if (d.kind == NNUE::AccDelta::Add && d.piece == piece && d.from == index) {
            --accPendingCount;
            return;
        }
    }
    if (accPendingCount == NNUE::MAX_ACC_PENDING) [[unlikely]] flushAccPending();
    accPending[accPendingCount++] = {NNUE::AccDelta::Remove, piece, index, 0};
}

inline void Board::queueAccMove(uint8_t piece, uint8_t fromIndex, uint8_t toIndex) const noexcept {
    if (accPendingCount > 0) {
        const NNUE::AccDelta& d = accPending[accPendingCount - 1];
        if (d.kind == NNUE::AccDelta::Move && d.piece == piece
            && d.from == toIndex && d.to == fromIndex) {
            --accPendingCount;
            return;
        }
    }
    if (accPendingCount == NNUE::MAX_ACC_PENDING) [[unlikely]] flushAccPending();
    accPending[accPendingCount++] = {NNUE::AccDelta::Move, piece, fromIndex, toIndex};
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
            for (int k = 0; k < ns; ++k)
                subRow[k] = nnueAccumulator.featureRow(q, static_cast<uint8_t>(sub[k] >> 8), static_cast<uint8_t>(sub[k]));
            for (int k = 0; k < na; ++k)
                addRow[k] = nnueAccumulator.featureRow(q, static_cast<uint8_t>(add[k] >> 8), static_cast<uint8_t>(add[k]));
            NNUE::Accumulator::applyRows(nnueAccumulator.v[q], subRow, ns, addRow, na);
            return;
        }
        // At most two subs and two adds per pass: those are the kernels that
        // exist, and one pass already covers four rows.
        for (int si = 0, ai = 0; si < ns || ai < na; ) {
            const int s = (ns - si > 2) ? 2 : ns - si;
            const int a = (na - ai > 2) ? 2 : na - ai;
            const int16_t* subRow[2][2];
            const int16_t* addRow[2][2];
            for (int k = 0; k < s; ++k) {
                const uint16_t c = sub[si + k];
                subRow[0][k] = nnueAccumulator.featureRow(0, static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c));
                subRow[1][k] = nnueAccumulator.featureRow(1, static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c));
            }
            for (int k = 0; k < a; ++k) {
                const uint16_t c = add[ai + k];
                addRow[0][k] = nnueAccumulator.featureRow(0, static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c));
                addRow[1][k] = nnueAccumulator.featureRow(1, static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c));
            }
            si += s;
            ai += a;
            switch (s * 4 + a) {
                case 2 * 4 + 2: nnueAccumulator.updateFused<2, 2>(subRow, addRow); break;
                case 2 * 4 + 1: nnueAccumulator.updateFused<2, 1>(subRow, addRow); break;
                case 1 * 4 + 2: nnueAccumulator.updateFused<1, 2>(subRow, addRow); break;
                case 1 * 4 + 1: nnueAccumulator.updateFused<1, 1>(subRow, addRow); break;
                case 2 * 4 + 0: nnueAccumulator.updateFused<2, 0>(subRow, addRow); break;
                case 0 * 4 + 2: nnueAccumulator.updateFused<0, 2>(subRow, addRow); break;
                case 1 * 4 + 0: nnueAccumulator.updateFused<1, 0>(subRow, addRow); break;
                default:        nnueAccumulator.updateFused<0, 1>(subRow, addRow); break;
            }
        }
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

__attribute__((always_inline))
inline void Board::addPieceToBB(uint8_t piece, uint8_t index) noexcept {
    dispatchPieceBBUpdate<true>(piece & MASK_PIECE_TYPE, colorToIndex(piece), BIT_MASKS[index]);
    if (NNUE::activeNetwork != nullptr) [[likely]] {
        queueAccAdd(piece, index);
    }
}

__attribute__((always_inline))
inline void Board::removePieceFromBB(uint8_t piece, uint8_t index) noexcept {
    dispatchPieceBBUpdate<false>(piece & MASK_PIECE_TYPE, colorToIndex(piece), BIT_MASKS[index]);
    if (NNUE::activeNetwork != nullptr) [[likely]] {
        queueAccRemove(piece, index);
    }
}

// Piece bitboards by Board::piece_id type - 1 (PAWN .. KING).
inline constexpr std::array<std::array<uint64_t, 2> Board::*, 6> PIECE_BB_BY_TYPE = {
    &Board::pawns_bb, &Board::knights_bb, &Board::bishops_bb,
    &Board::rooks_bb, &Board::queens_bb, &Board::kings_bb,
};

__attribute__((always_inline))
inline void Board::movePieceOnBB(uint8_t piece, uint8_t fromIndex, uint8_t toIndex) noexcept {
    const uint8_t type = piece & MASK_PIECE_TYPE;
    const uint8_t color = colorToIndex(piece);
    // The mover's own bitboard never holds the destination, so one XOR clears
    // the source and sets the destination, without the per-type switch.
    (this->*PIECE_BB_BY_TYPE[type - 1])[color] ^= BIT_MASKS[fromIndex] | BIT_MASKS[toIndex];
    if (NNUE::activeNetwork != nullptr) [[likely]] {
        queueAccMove(piece, fromIndex, toIndex);
    }
}

// From-scratch accumulator rebuild. Note rebuildBitboardsFromSquares goes
// through dispatchPieceBBUpdate directly (NOT addPieceToBB), so bulk rebuilds
// never double-count: they land here once at the end instead.
inline void Board::refreshNnueAccumulator() noexcept {
    if (NNUE::activeNetwork == nullptr) return;
    accPendingCount = 0;   // rebuilt from the squares below; queued work is moot
    // No kings (unit-test fragments, mid-load states): leave the accumulator
    // as-is; evaluation is guarded upstream by the kings-missing check.
    if (kings_bb[0] == 0 || kings_bb[1] == 0) return;
    const int wKingLerf      = std::countr_zero(kings_bb[0]) ^ 56;
    const int bKingLerfView  = std::countr_zero(kings_bb[1]); // lerf ^ 56
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
    if (kings_bb[0] == 0 || kings_bb[1] == 0) return;

    // Per-thread cache (Lazy SMP: each helper searches its own Board on its
    // own thread). Stale entries from other positions are still correct diff
    // bases - no invalidation needed, ever.
    static thread_local NNUE::FinnyTable finny;
    finny.ensureInitialised();

    const uint64_t cur[2][6] = {
        {pawns_bb[0], knights_bb[0], bishops_bb[0], rooks_bb[0], queens_bb[0], kings_bb[0]},
        {pawns_bb[1], knights_bb[1], bishops_bb[1], rooks_bb[1], queens_bb[1], kings_bb[1]},
    };

    for (int p = 0; p < 2; ++p) {
        if (!nnueAccumulator.dirty[p]) continue;
        // Own king square from this perspective's view: lerf for white,
        // lerf ^ 56 for black - which folds back to the raw engine index.
        const int engineKing = std::countr_zero(kings_bb[p]);
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
                uint64_t added   = cur[c][t] & ~e.bb[c][t];
                uint64_t removed = e.bb[c][t] & ~cur[c][t];
                while (added)
                    add[na++] = NNUE::Accumulator::rowFor(rowBase, rowFlip, p, piece,
                                                          static_cast<uint8_t>(pieces::popLSB(added)));
                while (removed)
                    sub[ns++] = NNUE::Accumulator::rowFor(rowBase, rowFlip, p, piece,
                                                          static_cast<uint8_t>(pieces::popLSB(removed)));
                e.bb[c][t] = cur[c][t];
            }
        }
        NNUE::Accumulator::applyRows(e.v, sub, ns, add, na);

        std::memcpy(nnueAccumulator.v[p], e.v, sizeof(e.v));
        nnueAccumulator.base[p] = rowBase;
        nnueAccumulator.flip[p] = static_cast<uint8_t>(rowFlip);
        nnueAccumulator.dirty[p] = false;
    }
}