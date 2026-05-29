//FIXME Usare i this in chiamate

// ==============================
// Compile-Time Basic Utilities
// ==============================

//FIXME Eliminare costati magiche
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
    //FIXME Creare 3 funzioni helper per racchiudere i blocchi di logica.
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
    incrementalNonPawnMajorCount = other.incrementalNonPawnMajorCount;
    incrementalPhaseWeight = other.incrementalPhaseWeight;
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
    //FIXME Eliminare costati magiche
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
    //FIXME Eliminare costati magiche
    return (chessboard[row] >> (col << 2)) & MASK_PIECE;
}

inline int32_t Board::getIncrementalPsqtDelta(bool isEndgame) const noexcept {
    const int32_t pawns = isEndgame ? incrementalPsqtPawnsEg : incrementalPsqtPawnsMg;
    const int32_t kings = isEndgame ? incrementalPsqtKingsEg : incrementalPsqtKingsMg;
    return incrementalPsqtPieces + pawns + kings;
}

inline void Board::getIncrementalPsqtMgEg(int32_t& outMg, int32_t& outEg) const noexcept {
    outMg = incrementalPsqtPieces + incrementalPsqtPawnsMg + incrementalPsqtKingsMg;
    outEg = incrementalPsqtPieces + incrementalPsqtPawnsEg + incrementalPsqtKingsEg;
}

template<uint32_t Term>
inline bool Board::hasEvalCacheTerm() const noexcept {
    return (evalCache.validMask & evalCacheBit(Term)) != 0;
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



__attribute__((always_inline))
inline constexpr uint8_t Board::getColor(uint8_t index) const noexcept {
    return (get(index) & MASK_COLOR) ? WHITE : BLACK;
}

__attribute__((hot, always_inline))
inline void Board::set(uint8_t index, piece_id value) noexcept {
    const uint8_t internal_row = 7 - (index >> 3);
    const uint8_t shift = (index & 7) << 2; // file * 4
    chessboard[internal_row] = (chessboard[internal_row] & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
}


// ==============================
// Board Internals
// ==============================

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
    incrementalNonPawnMajorCount = 0;
    incrementalPhaseWeight = 0;
    incrementalPsqtPawnsMg = 0;
    incrementalPsqtPawnsEg = 0;
    incrementalPsqtPieces = 0;
    incrementalPsqtKingsMg = 0;
    incrementalPsqtKingsEg = 0;

    //FIXME Contollare se non esiste qualche sistema per evitare il ciclo
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
        (occupancy & ~bitMask(fromIndex) & ~capturedMask) | bitMask(toIndex);
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

    //FIXME Usare this
    updateIncrementalEvalForPiece<PieceType, Add>(color, index);
}

template<uint8_t PieceType, bool Add>
inline void Board::updateIncrementalEvalForPiece(uint8_t color, uint8_t index) noexcept {
    const int32_t sideSign = (color == 0) ? 1 : -1;
    const int32_t signedDelta = Add ? sideSign : -sideSign;
    const uint8_t psqtIndex = (color == 0) ? index : engine::mirrorIndex(index);

    incrementalMaterialDelta += signedDelta * MATERIAL_VALUES[PieceType];
    
    if constexpr (PieceType == KNIGHT || PieceType == BISHOP || PieceType == ROOK || PieceType == QUEEN) {
        incrementalNonPawnMajorCount += (Add ? 1 : -1);
    }
    // PeSTO-style weighted phase units: N=B=1, R=2, Q=4 (max 24 across both sides).
    if constexpr (PieceType == KNIGHT || PieceType == BISHOP) {
        incrementalPhaseWeight += (Add ? 1 : -1);
    } else if constexpr (PieceType == ROOK) {
        incrementalPhaseWeight += (Add ? 2 : -2);
    } else if constexpr (PieceType == QUEEN) {
        incrementalPhaseWeight += (Add ? 4 : -4);
    }

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
    switch (pieceType) {
        case PAWN:   updatePieceTypeBB<PAWN, Add>(color, bit, index); break;
        case KNIGHT: updatePieceTypeBB<KNIGHT, Add>(color, bit, index); break;
        case BISHOP: updatePieceTypeBB<BISHOP, Add>(color, bit, index); break;
        case ROOK:   updatePieceTypeBB<ROOK, Add>(color, bit, index); break;
        case QUEEN:  updatePieceTypeBB<QUEEN, Add>(color, bit, index); break;
        case KING:   updatePieceTypeBB<KING, Add>(color, bit, index); break;
        default: break;
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

inline bool Board::isDraw(uint8_t color) const noexcept {
    return isStalemate(color) || isFiftyMoveRule() || isThreefoldRepetition() || hasInsufficientMaterialDraw();
}
