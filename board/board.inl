// ==============================
// Compile-Time Basic Utilities
// ==============================
inline constexpr uint8_t Board::oppositeColor(uint8_t color) noexcept { return color ^ 0x8; }

inline constexpr uint8_t Board::colorToIndex(uint8_t color) noexcept { return color >> 3; }

inline constexpr int Board::colorBoolToIndex(bool isWhite) noexcept { return isWhite ? 0 : 1; }

template<bool IsWhite>
inline constexpr uint8_t Board::promotionRank() noexcept { return IsWhite ? 0 : 7; }

inline constexpr uint8_t Board::promotionRank(bool isWhite) noexcept { return isWhite ? 0 : 7; }

// ==============================
// Constructors
// ==============================
inline Board::Board() noexcept {
    fromFenToBoard(STARTING_FEN);
}

inline Board::Board(const std::array<uint32_t, 8>& chessboard) noexcept
    : chessboard(chessboard)
    , castle(CASTLING_RIGHTS_ALL)
    , enPassant() 
    , halfMoveClock(0)
    , fullMoveClock(1)
    , activeColor(WHITE)
{
    updateOccupancyBB();
    rebuildRepetitionHistory();
}

inline Board::Board(const std::string& fen) {
    fromFenToBoard(fen);
}

// ==============================
// Geometry Helpers
// ==============================

inline constexpr uint64_t Board::bitMask(uint8_t sq) noexcept { return BIT_MASKS[sq]; }

inline constexpr uint8_t Board::fileOf(uint8_t sq) noexcept { return sq & 7; } // Extract bits 0-2 
inline constexpr uint8_t Board::rankOf(uint8_t sq) noexcept { return sq >> 3; } // Extract bits 3-5 

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

inline uint8_t Board::get(const std::string& square) const noexcept { 
    const uint8_t col = square[0] - 'a';
    const uint8_t row = square[1] - '1';
    return get(row, col);
}
    
inline std::string Board::getCurrentFen() const noexcept { return fromBoardToFen(); };

inline constexpr uint8_t Board::getActiveColor() const noexcept { return activeColor; }

inline Coords Board::getEnPassant() const noexcept { return enPassant; }

inline constexpr bool Board::getCastle(uint8_t index) const noexcept {
    return (castle & (1u << index));
}

__attribute__((always_inline))
inline constexpr uint8_t Board::getColor(const Coords& pos) const noexcept {
    return (get(pos) & MASK_COLOR) ? BLACK : WHITE;
}

__attribute__((always_inline))
inline constexpr uint8_t Board::getColor(uint8_t index) const noexcept {
    return (get(index) & MASK_COLOR) ? BLACK : WHITE;
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

__attribute__((always_inline))
inline void Board::set(uint8_t row, uint8_t col, piece_id value) noexcept {
    const uint8_t shift = col << 2; // col * 4
    chessboard[row] = (chessboard[row] & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
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
inline constexpr bool Board::isSameColor(const Coords& pos1, const Coords& pos2) const noexcept {
    uint8_t p1 = get(pos1);
    uint8_t p2 = get(pos2);
    if (p1 == EMPTY || p2 == EMPTY) return false;
    return (p1 & BLACK) == (p2 & BLACK);
}

__attribute__((always_inline))
inline void Board::updateChessboard(const Coords& from, const Coords& to, piece_id piece) noexcept {
    set(to, piece);
    set(from, EMPTY);
/*
    const uint8_t fromIndex = from.index;
    const uint8_t toIndex = to.index;
    
    // Direct array access usando index (evita conversioni ripetute)
    const uint8_t fromRank = fromIndex >> 3;
    const uint8_t fromFile = fromIndex & 7;
    const uint8_t toRank = toIndex >> 3;
    const uint8_t toFile = toIndex & 7;
    
    const uint8_t fromRow = 7 - fromRank;
    const uint8_t toRow = 7 - toRank;
    
    const uint8_t fromShift = fromFile << 2;
    const uint8_t toShift = toFile << 2;
    
    // Get piece from source (1 array access)
    const uint8_t piece = (chessboard[fromRow] >> fromShift) & MASK_PIECE;
    
    // Clear source and set destination (2 array writes)
    chessboard[fromRow] = (chessboard[fromRow] & ~(MASK_PIECE << fromShift));
    chessboard[toRow] = (chessboard[toRow] & ~(MASK_PIECE << toShift)) | ((piece & MASK_PIECE) << toShift);
  */
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

    // Single loop: iterate all 64 squares directly
    // index = rank * 8 + file, where rank 0 = row 8, rank 7 = row 1
    for (uint8_t index = 0; index < 64; ++index) {
        const uint8_t piece = get(index);
        
        if (piece == EMPTY) continue;
        
        const uint64_t bit = bitMask(index);
        const uint8_t color = piece >> 3; // Extract color bit directly (bit 3)

        occupancy |= bit;
        dispatchPieceBBUpdate<true>(piece & MASK_PIECE_TYPE, color, bit);
    }
}

__attribute__((always_inline))
inline void Board::fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept {
    occupancy |= bitMask(toIndex);  // Set the bit at 'to' position    
    occupancy &= ~bitMask(fromIndex); // Clear the bit at 'from' position
}

inline bool Board::hasAtLeastTwoBits(uint64_t bb) noexcept { return (bb & (bb - 1)) != 0ULL; }

inline bool Board::addAttackAndDetectDouble(uint64_t attackSet, uint8_t& attackers) noexcept {
    if (!attackSet) return false;
    if (hasAtLeastTwoBits(attackSet)) return true;
    ++attackers;
    return attackers >= 2;
}

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

__attribute__((always_inline))
inline void Board::addPieceToBB(uint8_t piece, uint8_t index) noexcept {
    if (piece == EMPTY) [[unlikely]] return;
    const uint8_t color = static_cast<uint8_t>((piece & MASK_COLOR) >> 3); // BLACK=1, WHITE=0
    const uint64_t bit = bitMask(index);
    dispatchPieceBBUpdate<true>(piece & MASK_PIECE_TYPE, color, bit);
}

__attribute__((always_inline))
inline void Board::removePieceFromBB(uint8_t piece, uint8_t index) noexcept {
    if (piece == EMPTY) [[unlikely]] return;
    const uint8_t color = static_cast<uint8_t>((piece & MASK_COLOR) >> 3); // BLACK=1, WHITE=0
    const uint64_t bit = bitMask(index);
    dispatchPieceBBUpdate<false>(piece & MASK_PIECE_TYPE, color, bit);
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