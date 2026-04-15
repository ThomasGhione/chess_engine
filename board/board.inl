// ==============================
// Compile-Time Basic Utilities
// ==============================

inline constexpr uint8_t Board::oppositeColor(uint8_t color) noexcept { return color ^ 0x8; }

inline constexpr uint8_t Board::colorToIndex(uint8_t color) noexcept {
    return ((color & MASK_COLOR) >> 3) ^ 0x1u;
}

inline constexpr uint8_t Board::promotionRank(bool isWhite) noexcept { return isWhite ? 0 : 7; }

// ==============================
// Constructors
// ==============================
inline Board::Board() noexcept {
    fromFenToBoard(STARTING_FEN);
}

inline Board::Board(const std::array<uint32_t, 8>& chessboard) noexcept
    : chessboard(chessboard)
    , halfMoveClock(0)
    , fullMoveClock(1)
    , castle(CASTLING_RIGHTS_ALL)
    , enPassant()
    , activeColor(WHITE)
{
    updateOccupancyBB();
    rebuildRepetitionHistory();
}

inline Board::Board(const std::string& fen) {
    fromFenToBoard(fen);
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

inline Board::Board(Board&& other) noexcept {
    copyFromBoard(other);
}

inline Board& Board::operator=(Board&& other) noexcept {
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
    incrementalMaterialDelta = other.incrementalMaterialDelta;
    incrementalPsqtPawnsMg = other.incrementalPsqtPawnsMg;
    incrementalPsqtPawnsEg = other.incrementalPsqtPawnsEg;
    incrementalPsqtPieces = other.incrementalPsqtPieces;
    incrementalPsqtKingsMg = other.incrementalPsqtKingsMg;
    incrementalPsqtKingsEg = other.incrementalPsqtKingsEg;
    evalCache = other.evalCache;
    lastMoveChangeFlags = other.lastMoveChangeFlags;
    halfMoveClock = other.halfMoveClock;
    fullMoveClock = other.fullMoveClock;
    castle = other.castle;
    hasMoved = other.hasMoved;
    enPassant = other.enPassant;
    epHashFile = other.epHashFile;
    activeColor = other.activeColor;
}

// ==============================
// Geometry Helpers
// ==============================

inline constexpr uint64_t Board::bitMask(uint8_t sq) noexcept { return BIT_MASKS[sq]; }

inline constexpr uint8_t Board::file(uint8_t sq) noexcept { return chess::file(sq); } // Extract bits 0-2
inline constexpr uint8_t Board::rank(uint8_t sq) noexcept { return chess::rank(sq); } // Extract bits 3-5

// ==============================
// Move Value Helpers
// ==============================
inline bool Board::Move::operator==(const Move& other) const noexcept {
    return from == other.from && to == other.to && promotionPiece == other.promotionPiece;
}

template<typename MoveContainer>
inline void Board::Move::rotate(MoveContainer& moves, size_t index) noexcept {
    Move temp = moves[index];
    // Shift all moves [0..index-1] one position to the right
    for (size_t i = index; i > 0; --i) {
        moves[i] = moves[i - 1];
    }
    moves[0] = temp;
}

inline std::string Board::Move::toUCIString() const noexcept {
    std::string uciMove = from.toString() + to.toString();
    if (promotionPiece != '\0') {
        uciMove += std::tolower(promotionPiece);
    }
    return uciMove;
}

// ==============================
// Public Board API
// ==============================
__attribute__((hot, always_inline))
inline constexpr uint8_t Board::get(uint8_t index) const noexcept {
    const uint8_t rank = index >> 3;  // index / 8 (Coords convention)
    const uint8_t file = index & 7;   // index % 8
    // Convert from Coords convention to Board storage
    return (chessboard[7 - rank] >> (file << 2)) & MASK_PIECE;
}

__attribute__((always_inline))
inline constexpr uint8_t Board::get(Coords coords) const noexcept {
    return get(coords.index);
}

__attribute__((always_inline))
inline constexpr uint8_t Board::get(uint8_t row, uint8_t col) const noexcept {
    return (chessboard[row] >> (col << 2)) & MASK_PIECE;
}

inline constexpr uint8_t Board::getActiveColor() const noexcept { return activeColor; }

inline Coords Board::getEnPassant() const noexcept { return enPassant; }

inline constexpr int32_t Board::getIncrementalMaterialDelta() const noexcept {
    return incrementalMaterialDelta;
}

inline int32_t Board::getIncrementalPsqtDelta(bool isEndgame) const noexcept {
    const int32_t pawns = isEndgame ? incrementalPsqtPawnsEg : incrementalPsqtPawnsMg;
    const int32_t kings = isEndgame ? incrementalPsqtKingsEg : incrementalPsqtKingsMg;
    return incrementalPsqtPieces + pawns + kings;
}

inline bool Board::hasEvalCacheTerm(uint32_t term) const noexcept {
    return (evalCache.validMask & evalCacheBit(term)) != 0;
}

template<uint32_t Term>
inline bool Board::hasEvalCacheTerm() const noexcept {
    return (evalCache.validMask & evalCacheBit(Term)) != 0;
}

inline int32_t Board::getEvalCacheTerm(uint32_t term) const noexcept {
    return evalCache.terms[term];
}

template<uint32_t Term>
inline int32_t& Board::evalCacheTermRef() const noexcept {
    static_assert(Term < EVAL_CACHE_COUNT, "Unsupported eval cache term");
    return evalCache.terms[Term];
}

template<uint32_t Term>
inline int32_t Board::getEvalCacheTerm() const noexcept {
    return evalCacheTermRef<Term>();
}

inline void Board::setEvalCacheTerm(uint32_t term, int32_t value) const noexcept {
    evalCache.terms[term] = value;
    evalCache.validMask |= evalCacheBit(term);
}

template<uint32_t Term>
inline void Board::setEvalCacheTerm(int32_t value) const noexcept {
    evalCacheTermRef<Term>() = value;
    evalCache.validMask |= evalCacheBit(Term);
}

inline void Board::invalidateEvalCacheTerms(uint32_t terms) noexcept {
    evalCache.validMask &= ~terms;
}

inline void Board::clearEvalCache() noexcept {
    evalCache.validMask = 0;
}

inline constexpr bool Board::getCastle(uint8_t index) const noexcept {
    return (castle & (1u << index));
}

__attribute__((always_inline))
inline constexpr uint8_t Board::getColor(const Coords& pos) const noexcept {
    return (get(pos) & MASK_COLOR) ? WHITE : BLACK;
}

__attribute__((always_inline))
inline constexpr uint8_t Board::getColor(uint8_t index) const noexcept {
    return (get(index) & MASK_COLOR) ? WHITE : BLACK;
}

inline constexpr uint16_t Board::getFullMoveClock() const noexcept { return fullMoveClock; }

__attribute__((hot, always_inline))
inline void Board::set(uint8_t index, piece_id value) noexcept {
    const uint8_t internal_row = 7 - (index >> 3);
    const uint8_t shift = (index & 7) << 2; // file * 4
    chessboard[internal_row] = (chessboard[internal_row] & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
}
    
__attribute__((always_inline))
inline void Board::set(Coords coords, piece_id value) noexcept {
    set(coords.index, value);
}


inline constexpr uint8_t Board::operator[](const Coords& coords) const noexcept { return get(coords); }
inline uint8_t Board::operator[](const Coords& coords) noexcept { return get(coords); }
inline constexpr uint8_t Board::operator[](uint8_t index) const noexcept { return get(index); } // assert index 0-63
inline uint8_t Board::operator[](uint8_t index) noexcept { return get(index); }
inline constexpr bool Board::operator==(const Board& other) const noexcept { return chessboard == other.chessboard; }
inline constexpr bool Board::operator!=(const Board& other) const noexcept { return chessboard != other.chessboard; }

// ==============================
// Board Internals
// ==============================

__attribute__((always_inline))
inline void Board::updateChessboard(const Coords& from, const Coords& to, piece_id piece) noexcept {
    set(to, piece);
    set(from, EMPTY);
}

inline uint64_t Board::getPiecesBitMap() const noexcept { return occupancy; }

inline void Board::updateOccupancyBB() noexcept {
    // Reset all bitboards
    occupancy       = 0ULL;
    pawns_bb[0]     = pawns_bb[1]     = 0ULL;
    knights_bb[0]   = knights_bb[1]   = 0ULL;
    bishops_bb[0]   = bishops_bb[1]   = 0ULL;
    rooks_bb[0]     = rooks_bb[1]     = 0ULL;
    queens_bb[0]    = queens_bb[1]    = 0ULL;
    kings_bb[0]     = kings_bb[1]     = 0ULL;
    incrementalMaterialDelta = 0;
    incrementalPsqtPawnsMg = 0;
    incrementalPsqtPawnsEg = 0;
    incrementalPsqtPieces = 0;
    incrementalPsqtKingsMg = 0;
    incrementalPsqtKingsEg = 0;

    // Single loop: iterate all 64 squares directly
    // index = rank * 8 + file, where rank 0 = row 8, rank 7 = row 1
    for (uint8_t index = 0; index < 64; ++index) {
        const uint8_t piece = get(index);
        
        if (piece == EMPTY) continue;
        
        const uint64_t bit = bitMask(index);
        const uint8_t color = colorToIndex(piece & MASK_COLOR);

        occupancy |= bit;
        dispatchPieceBBUpdate<true>(piece & MASK_PIECE_TYPE, color, bit, index);
    }
}

__attribute__((always_inline))
inline void Board::fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept {
    occupancy |= bitMask(toIndex);  // Set the bit at 'to' position    
    occupancy &= ~bitMask(fromIndex); // Clear the bit at 'from' position
}

inline bool Board::hasAtLeastTwoBits(uint64_t bb) noexcept { return (bb & (bb - 1)) != 0ULL; }

inline bool Board::isKingSafeAfterMove(
    uint8_t movingColor,
    uint8_t fromIndex,
    uint8_t toIndex,
    uint64_t capturedEnemyMask
) const noexcept {
    const uint8_t side = colorToIndex(movingColor);
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return false;

    const uint8_t oppSide = side ^ 1;
    const uint8_t kingSq = __builtin_ctzll(kingBB);

    uint64_t occNew = occupancy;
    occNew &= ~bitMask(fromIndex);
    occNew |= bitMask(toIndex);

    if (capturedEnemyMask == 0ULL) {
        return !isKingAttackedCustom(kingSq, oppSide, occNew,
                                     pawns_bb[oppSide],
                                     knights_bb[oppSide],
                                     bishops_bb[oppSide],
                                     rooks_bb[oppSide],
                                     queens_bb[oppSide],
                                     kings_bb[oppSide]);
    }

    return !isKingAttackedCustom(kingSq, oppSide, occNew,
                                 pawns_bb[oppSide] & ~capturedEnemyMask,
                                 knights_bb[oppSide] & ~capturedEnemyMask,
                                 bishops_bb[oppSide] & ~capturedEnemyMask,
                                 rooks_bb[oppSide] & ~capturedEnemyMask,
                                 queens_bb[oppSide] & ~capturedEnemyMask,
                                 kings_bb[oppSide] & ~capturedEnemyMask);
}

inline bool Board::isKingSafeAfterEnPassant(
    uint8_t movingColor,
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t capturedPawnIndex
) const noexcept {
    const uint8_t side = colorToIndex(movingColor);
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return false;

    const uint8_t oppSide = side ^ 1;
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    const uint64_t capturedPawnMask = bitMask(capturedPawnIndex);

    uint64_t occNew = occupancy;
    occNew &= ~bitMask(fromIndex);
    occNew &= ~capturedPawnMask;
    occNew |= bitMask(toIndex);

    return !isKingAttackedCustom(kingSq, oppSide, occNew,
                                 pawns_bb[oppSide] & ~capturedPawnMask,
                                 knights_bb[oppSide],
                                 bishops_bb[oppSide],
                                 rooks_bb[oppSide],
                                 queens_bb[oppSide],
                                 kings_bb[oppSide]);
}

template<uint8_t PieceType, bool Add>
inline void Board::updatePieceTypeBB(uint8_t color, uint64_t bit, uint8_t index) noexcept {
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

    updateIncrementalEvalForPiece<PieceType, Add>(color, index);
}

template<uint8_t PieceType, bool Add>
inline void Board::updateIncrementalEvalForPiece(uint8_t color, uint8_t index) noexcept {
    const int32_t sideSign = (color == 0) ? 1 : -1;
    const int32_t signedDelta = Add ? sideSign : -sideSign;
    const uint8_t psqtIndex = (color == 0) ? index : engine::mirrorIndex(index);

    incrementalMaterialDelta += signedDelta * MATERIAL_VALUES[PieceType];

    if constexpr (PieceType == PAWN) {
        incrementalPsqtPawnsMg += signedDelta * engine::PAWN_VALUES_TABLE[psqtIndex];
        incrementalPsqtPawnsEg += signedDelta * engine::PAWN_END_GAME_VALUES_TABLE[psqtIndex];
    } else if constexpr (PieceType == KNIGHT) {
        incrementalPsqtPieces += signedDelta * engine::KNIGHT_VALUES_TABLE[psqtIndex];
    } else if constexpr (PieceType == BISHOP) {
        incrementalPsqtPieces += signedDelta * engine::BISHOP_VALUES_TABLE[psqtIndex];
    } else if constexpr (PieceType == ROOK) {
        incrementalPsqtPieces += signedDelta * engine::ROOK_VALUES_TABLE[psqtIndex];
    } else if constexpr (PieceType == QUEEN) {
        incrementalPsqtPieces += signedDelta * engine::QUEEN_VALUES_TABLE[psqtIndex];
    } else if constexpr (PieceType == KING) {
        incrementalPsqtKingsMg += signedDelta * engine::KING_MIDDLE_GAME_VALUES_TABLE[psqtIndex];
        incrementalPsqtKingsEg += signedDelta * engine::KING_END_GAME_VALUES_TABLE[psqtIndex];
    }
}

template<bool Add>
inline void Board::dispatchPieceBBUpdate(uint8_t pieceType, uint8_t color, uint64_t bit, uint8_t index) noexcept {
    using UpdateFunc = void (Board::*)(uint8_t, uint64_t, uint8_t) noexcept;
    static constexpr UpdateFunc lut[8] = {
        nullptr,
        &Board::updatePieceTypeBB<PAWN, Add>,
        &Board::updatePieceTypeBB<KNIGHT, Add>,
        &Board::updatePieceTypeBB<BISHOP, Add>,
        &Board::updatePieceTypeBB<ROOK, Add>,
        &Board::updatePieceTypeBB<QUEEN, Add>,
        &Board::updatePieceTypeBB<KING, Add>,
        nullptr
    };

    if (auto func = lut[pieceType]) [[likely]] {
        (this->*func)(color, bit, index);
    }
}

__attribute__((always_inline))
inline void Board::addPieceToBB(uint8_t piece, uint8_t index) noexcept {
    if (piece == EMPTY) [[unlikely]] return;
    const uint8_t color = colorToIndex(piece & MASK_COLOR);
    const uint64_t bit = bitMask(index);
    dispatchPieceBBUpdate<true>(piece & MASK_PIECE_TYPE, color, bit, index);
}

__attribute__((always_inline))
inline void Board::removePieceFromBB(uint8_t piece, uint8_t index) noexcept {
    if (piece == EMPTY) [[unlikely]] return;
    const uint8_t color = colorToIndex(piece & MASK_COLOR);
    const uint64_t bit = bitMask(index);
    dispatchPieceBBUpdate<false>(piece & MASK_PIECE_TYPE, color, bit, index);
}

// ==============================
// High-Level Game State API
// ==============================
__attribute__((hot))
inline bool Board::isCheckmate(uint8_t color) const noexcept {return inCheck(color) && !hasAnyLegalMove(color);}

inline bool Board::isStalemate(uint8_t color) const noexcept {return !inCheck(color) && !hasAnyLegalMove(color);}

inline bool Board::isFiftyMoveRule() const noexcept { return halfMoveClock >= 100; }

inline bool Board::isDraw(uint8_t color) const noexcept {
    return isStalemate(color) || isFiftyMoveRule() || isThreefoldRepetition();
}
