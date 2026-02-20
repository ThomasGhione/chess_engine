inline constexpr uint8_t Board::oppositeColor(uint8_t color) noexcept { return color ^ 0x8; }

inline constexpr uint8_t Board::colorToIndex(uint8_t color) noexcept { return color >> 3; }

inline constexpr int Board::colorBoolToIndex(bool isWhite) noexcept { return isWhite ? 0 : 1; }

template<bool IsWhite>
inline constexpr uint8_t Board::promotionRank() noexcept { return IsWhite ? 0 : 7; }

inline constexpr uint8_t Board::promotionRank(bool isWhite) noexcept { return isWhite ? 0 : 7; }

inline constexpr bool Board::isPromotionRank(uint8_t rank, bool isWhite) noexcept { return rank == promotionRank(isWhite); }

template<bool IsWhite>
inline constexpr uint8_t Board::backRank() noexcept { return IsWhite ? 0 : 7; }

template<bool IsWhite>
inline constexpr uint8_t Board::seventhRank() noexcept { return IsWhite ? 6 : 1; }

inline constexpr bool Board::isCaptureKind(MoveKind kind) noexcept {
    return kind == MoveKind::Capture || kind == MoveKind::PromotionCapture;
}

inline constexpr bool Board::isPromotionKind(MoveKind kind) noexcept {
    return kind == MoveKind::PromotionQuiet || kind == MoveKind::PromotionCapture;
}

inline Board::MoveKind Board::classifyMoveKind(
    uint8_t movingType,
    uint8_t movingColor,
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t destBefore,
    const Coords& prevEnPassant
) noexcept {
    const uint8_t fromFile = fromIndex & 7;
    const uint8_t fromRank = fromIndex >> 3;
    const uint8_t toFile = toIndex & 7;
    const uint8_t toRank = toIndex >> 3;

    if (movingType == KING && fromRank == toRank) {
        const int8_t df = static_cast<int8_t>(toFile - fromFile);
        if (df == 2 || df == -2) {
            return MoveKind::Castling;
        }
    }

    if (movingType == PAWN) {
        const bool isEnPassant = (fromFile != toFile)
            && (destBefore == EMPTY)
            && Coords::isInBounds(prevEnPassant)
            && (toIndex == prevEnPassant.index);
        if (isEnPassant) {
            return MoveKind::EnPassant;
        }

        const bool isPromotion = (toRank == promotionRank(movingColor == WHITE));
        if (isPromotion) {
            return (destBefore != EMPTY) ? MoveKind::PromotionCapture : MoveKind::PromotionQuiet;
        }

        const int8_t dr = static_cast<int8_t>(toRank - fromRank);
        if (dr == 2 || dr == -2) {
            return MoveKind::DoublePawnPush;
        }
    }

    if (destBefore != EMPTY) {
        return MoveKind::Capture;
    }

    return MoveKind::Quiet;
}

inline uint8_t Board::normalizePromotionChoice(char promotionChoice) noexcept {
    const uint8_t promo = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(promotionChoice)));
    if (promo == 'q' || promo == 'r' || promo == 'b' || promo == 'n') {
        return promo;
    }
    return static_cast<uint8_t>('q');
}

inline uint8_t Board::promotedPieceFromChoice(uint8_t promo, uint8_t movingColor) noexcept {
    if (promo == 'r') return ROOK | movingColor;
    if (promo == 'b') return BISHOP | movingColor;
    if (promo == 'n') return KNIGHT | movingColor;
    return QUEEN | movingColor;
}

template<Board::MoveKind Kind>
inline void Board::doMoveByKind(
    const Coords& from,
    const Coords& to,
    MoveState& st,
    uint8_t moving,
    uint8_t movingType,
    uint8_t movingColor,
    uint8_t destBefore,
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t fromFile,
    uint8_t fromRank,
    uint8_t toFile,
    uint8_t toRank,
    char promotionChoice
) noexcept {
    if constexpr (Kind == MoveKind::EnPassant) {
        st.wasEnPassantCapture = true;
        const int8_t captureOffset = (movingColor == WHITE) ? 8 : -8;
        const uint8_t capIndex = static_cast<uint8_t>(toIndex + captureOffset);
        const uint8_t capturedPiece = get(capIndex);
        st.capturedPiece = capturedPiece;
        st.enPassantCapturedIndex = capIndex;

        set(capIndex, EMPTY);
        occupancy &= ~bitMask(capIndex);
        removePieceFromBB(capturedPiece, capIndex);
    }

    if constexpr (isCaptureKind(Kind)) {
        removePieceFromBB(destBefore, toIndex);
    }

    updateChessboard(from, to, static_cast<piece_id>(moving));
    fastUpdateOccupancyBB(fromIndex, toIndex);
    removePieceFromBB(moving, fromIndex);
    addPieceToBB(moving, toIndex);

    if constexpr (Kind == MoveKind::Castling) {
        st.wasCastling = true;
        const uint8_t rookFromFile = (toFile > fromFile) ? 7 : 0;
        const uint8_t rookToFile   = (toFile > fromFile) ? 5 : 3;
        const uint8_t rookFromIndex = static_cast<uint8_t>((toRank << 3) | rookFromFile);
        const uint8_t rookToIndex   = static_cast<uint8_t>((toRank << 3) | rookToFile);
        st.rookFromIndex = rookFromIndex;
        st.rookToIndex   = rookToIndex;

        const uint8_t rook = get(rookFromIndex);
        set(rookToIndex, static_cast<piece_id>(rook));
        set(rookFromIndex, EMPTY);
        fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
        removePieceFromBB(rook, rookFromIndex);
        addPieceToBB(rook, rookToIndex);
    }

    if (movingType == KING) {
        const uint8_t kingBit = (movingColor == WHITE) ? 0x01 : 0x08;
        const uint8_t castleMask = (movingColor == WHITE) ? 0x03 : 0x0C;
        castle &= ~castleMask;
        hasMoved |= kingBit;
    } else if (movingType == ROOK) {
        const bool isInitialSquare = (movingColor == WHITE)
            ? (fromRank == 7 && (fromFile == 0 || fromFile == 7))
            : (fromRank == 0 && (fromFile == 0 || fromFile == 7));

        if (isInitialSquare) {
            if (movingColor == WHITE) {
                if (fromFile == 0) {
                    castle &= ~(1u << 1);
                    hasMoved |= (1u << 1);
                } else {
                    castle &= ~(1u << 0);
                    hasMoved |= (1u << 2);
                }
            } else {
                if (fromFile == 0) {
                    castle &= ~(1u << 3);
                    hasMoved |= (1u << 4);
                } else {
                    castle &= ~(1u << 2);
                    hasMoved |= (1u << 5);
                }
            }
        }
    }

    if constexpr (isCaptureKind(Kind)) {
        if ((destBefore & MASK_PIECE_TYPE) == ROOK) {
            const bool isInitialSquare = ((destBefore & MASK_COLOR) == WHITE)
                ? (toRank == 7 && (toFile == 0 || toFile == 7))
                : (toRank == 0 && (toFile == 0 || toFile == 7));

            if (isInitialSquare) {
                if ((destBefore & MASK_COLOR) == WHITE) {
                    castle &= (toFile == 0) ? ~(1u << 1) : ~(1u << 0);
                } else {
                    castle &= (toFile == 0) ? ~(1u << 3) : ~(1u << 2);
                }
            }
        }
    }

    if constexpr (Kind == MoveKind::DoublePawnPush) {
        enPassant = Coords{fromFile, static_cast<uint8_t>((fromRank + toRank) >> 1)};
    }

    if constexpr (isPromotionKind(Kind)) {
        const uint8_t promo = normalizePromotionChoice(promotionChoice);
        st.promotionPieceType = promo;
        (void)promote(to, static_cast<char>(promo));
    }
}

template<Board::MoveKind Kind>
inline void Board::undoMoveByKind(
    const Coords& from,
    const Coords& to,
    const MoveState& st,
    uint8_t& pieceOnTo,
    uint8_t fromIndex,
    uint8_t toIndex
) noexcept {
    if constexpr (isPromotionKind(Kind)) {
        const uint8_t color = pieceOnTo & MASK_COLOR;
        const uint8_t pawnPiece = (PAWN | color);
        removePieceFromBB(pieceOnTo, toIndex);
        addPieceToBB(pawnPiece, toIndex);
        set(toIndex, static_cast<piece_id>(pawnPiece));
        pieceOnTo = pawnPiece;
    }

    updateChessboard(to, from, static_cast<piece_id>(pieceOnTo));
    fastUpdateOccupancyBB(toIndex, fromIndex);
    removePieceFromBB(pieceOnTo, toIndex);
    addPieceToBB(pieceOnTo, fromIndex);

    if constexpr (Kind == MoveKind::EnPassant) {
        const uint8_t capIndex = st.enPassantCapturedIndex;
        set(capIndex, static_cast<piece_id>(st.capturedPiece));
        occupancy |= bitMask(capIndex);
        addPieceToBB(st.capturedPiece, capIndex);
    } else if constexpr (isCaptureKind(Kind)) {
        if (st.capturedPiece != EMPTY) {
            set(toIndex, static_cast<piece_id>(st.capturedPiece));
            occupancy |= bitMask(toIndex);
            addPieceToBB(st.capturedPiece, toIndex);
        }
    }

    if constexpr (Kind == MoveKind::Castling) {
        const uint8_t rookFromIndex = st.rookFromIndex;
        const uint8_t rookToIndex   = st.rookToIndex;
        const uint8_t rook = get(rookToIndex);
        set(rookFromIndex, static_cast<piece_id>(rook));
        set(rookToIndex, EMPTY);
        fastUpdateOccupancyBB(rookToIndex, rookFromIndex);
        removePieceFromBB(rook, rookToIndex);
        addPieceToBB(rook, rookFromIndex);
    }
}

inline constexpr uint8_t Board::verticalMirror(uint8_t sq) noexcept {
    return sq ^ 56;  // XOR with 0b111000 flips bits 3-5 (rank)
}

inline constexpr uint8_t Board::horizontalMirror(uint8_t sq) noexcept {
    return sq ^ 7;   // XOR with 0b000111 flips bits 0-2 (file)
}

inline constexpr uint64_t Board::fileMask(int file) noexcept {
    return FILE_MASKS[file];
}

inline constexpr uint64_t Board::rankMask(int rank) noexcept {
    return RANK_MASKS[rank];
}

inline constexpr uint64_t Board::fileMaskFromSquare(uint8_t sq) noexcept {
    return FILE_MASKS[sq & 7];  // Extract file (bits 0-2)
}

inline constexpr uint64_t Board::rankMaskFromSquare(uint8_t sq) noexcept {
    return RANK_MASKS[sq >> 3];  // Extract rank (bits 3-5)
}

inline constexpr uint64_t Board::bitMask(uint8_t sq) noexcept { return BIT_MASKS[sq]; }

inline constexpr uint8_t Board::fileOf(uint8_t sq) noexcept {
    return sq & 7;  // Extract bits 0-2
}

inline constexpr uint8_t Board::rankOf(uint8_t sq) noexcept {
    return sq >> 3;  // Extract bits 3-5
}

inline bool Board::Move::operator==(const Move& other) const noexcept {
    return from == other.from && to == other.to && promotionPiece == other.promotionPiece;
}

template<typename MoveContainer>
inline void Board::Move::rotate(MoveContainer& moves, size_t index) noexcept {
    Move temp = moves[index];
    // Shifta tutte le mosse [0..index-1] una posizione a destra
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

inline constexpr bool Board::getCastle(uint8_t index) const noexcept {
    return (castle & (1u << index));
}

inline constexpr bool Board::getHasMoved(uint8_t index) const noexcept {
    return (hasMoved & (1u << index));
}

__attribute__((always_inline))
inline constexpr uint8_t Board::getColor(const Coords& pos) const noexcept {
    return (get(pos) & MASK_COLOR) ? BLACK : WHITE;
}

__attribute__((always_inline))
inline constexpr uint8_t Board::getColor(uint8_t index) const noexcept {
    return (get(index) & MASK_COLOR) ? BLACK : WHITE;
}

inline constexpr uint16_t Board::getHalfMoveClock() const noexcept { return halfMoveClock; }
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

inline void Board::setNextTurn() noexcept {
    if (activeColor == WHITE) {
        activeColor = BLACK;
    } else {
        activeColor = WHITE;        
        ++fullMoveClock;
    }
}

inline void Board::setPrevTurn() noexcept {
    if (activeColor == BLACK) {
        activeColor = WHITE;
    } else {
        activeColor = BLACK;
        if (fullMoveClock > 1) {
            fullMoveClock--;
        }
    }
    if (halfMoveClock > 0) {
        halfMoveClock--;
    }
}

inline constexpr uint8_t Board::operator[](const Coords& coords) const noexcept { return get(coords); }
inline uint8_t Board::operator[](const Coords& coords) noexcept { return get(coords); }
inline constexpr uint8_t Board::operator[](uint8_t index) const noexcept { return get(index); } // assert index 0-63
inline uint8_t Board::operator[](uint8_t index) noexcept { return get(index); }
inline constexpr bool Board::operator==(const Board& other) const noexcept { return chessboard == other.chessboard; }
inline constexpr bool Board::operator!=(const Board& other) const noexcept { return chessboard != other.chessboard; }

inline constexpr size_t Board::CHESSBOARD_SIZE() noexcept { return sizeof(chessboard); } // 32 byte

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

inline uint64_t Board::getPiecesBitMap() const noexcept {
    // Occupancy bitboard already tracks all non-empty squares.
    return occupancy;
}

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

        switch (piece & MASK_PIECE_TYPE) {
            case PAWN:   pawns_bb[color]   |= bit; break;
            case KNIGHT: knights_bb[color] |= bit; break;
            case BISHOP: bishops_bb[color] |= bit; break;
            case ROOK:   rooks_bb[color]   |= bit; break;
            case QUEEN:  queens_bb[color]  |= bit; break;
            case KING:   kings_bb[color]   |= bit; break;
        }
    }
}

__attribute__((always_inline))
inline void Board::fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept {
    occupancy |= bitMask(toIndex);  // Set the bit at 'to' position    
    occupancy &= ~bitMask(fromIndex); // Clear the bit at 'from' position
}

__attribute__((always_inline))
inline void Board::addPieceToBB(uint8_t piece, uint8_t index) noexcept {
    if (piece == EMPTY) return;
    const uint8_t color = (piece & MASK_COLOR) != 0; // BLACK=1, WHITE=0
    const uint64_t bit = (bitMask(index));
    switch (piece & MASK_PIECE_TYPE) {
        case PAWN:   pawns_bb[color]   |= bit; break;
        case KNIGHT: knights_bb[color] |= bit; break;
        case BISHOP: bishops_bb[color] |= bit; break;
        case ROOK:   rooks_bb[color]   |= bit; break;
        case QUEEN:  queens_bb[color]  |= bit; break;
        case KING:   kings_bb[color]   |= bit; break;
    }
}

__attribute__((always_inline))
inline void Board::removePieceFromBB(uint8_t piece, uint8_t index) noexcept {
    if (piece == EMPTY) return;
    const uint8_t color = (piece & MASK_COLOR) != 0;
    const uint64_t mask = ~(bitMask(index));
    switch (piece & MASK_PIECE_TYPE) {
        case PAWN:   pawns_bb[color]   &= mask; break;
        case KNIGHT: knights_bb[color] &= mask; break;
        case BISHOP: bishops_bb[color] &= mask; break;
        case ROOK:   rooks_bb[color]   &= mask; break;
        case QUEEN:  queens_bb[color]  &= mask; break;
        case KING:   kings_bb[color]   &= mask; break;
    }
}

__attribute__((hot))
inline bool Board::isCheckmate(uint8_t color) const noexcept {return inCheck(color) && !hasAnyLegalMove(color);}

inline bool Board::isStalemate(uint8_t color) const noexcept {return !inCheck(color) && !hasAnyLegalMove(color);}

inline bool Board::isFiftyMoveRule() const noexcept { return halfMoveClock >= 100; }

inline bool Board::isDraw(uint8_t color) const noexcept {
    return isStalemate(color) || isFiftyMoveRule() || isThreefoldRepetition();
}

inline Coords Board::getEnPassant() const noexcept {
    return enPassant;
}
