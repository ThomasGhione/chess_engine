// --------------------------
// API Helpers (engineapi)
// --------------------------

__attribute__((always_inline))
inline constexpr bool Board::isCaptureKind(MoveKind kind) noexcept {
    return kind == MoveKind::Capture || kind == MoveKind::PromotionCapture;
}

__attribute__((always_inline))
inline constexpr bool Board::isPromotionKind(MoveKind kind) noexcept {
    return kind == MoveKind::PromotionQuiet || kind == MoveKind::PromotionCapture;
}

inline uint32_t Board::computeMoveChangeFlags(const MoveState& st) noexcept {
    uint32_t flags = MOVE_CHANGE_NONE;

    if (isCaptureKind(st.moveKind) || st.wasEnPassantCapture) {
        flags |= MOVE_CHANGE_CAPTURE;
    }
    if (isPromotionKind(st.moveKind)) {
        flags |= MOVE_CHANGE_PROMOTION;
    }
    if (st.moveKind == MoveKind::Castling) {
        flags |= MOVE_CHANGE_CASTLING;
        // Castling also moves a rook, which affects rook/file and coordination terms.
        flags |= MOVE_CHANGE_ROOK_MOVE;
    }

    switch (st.fromPiece & MASK_PIECE_TYPE) {
        case PAWN:   flags |= MOVE_CHANGE_PAWN_MOVE; break;
        case KNIGHT: flags |= MOVE_CHANGE_KNIGHT_MOVE; break;
        case BISHOP: flags |= MOVE_CHANGE_BISHOP_MOVE; break;
        case ROOK:   flags |= MOVE_CHANGE_ROOK_MOVE; break;
        case QUEEN:  flags |= MOVE_CHANGE_QUEEN_MOVE; break;
        case KING:   flags |= MOVE_CHANGE_KING_MOVE; break;
        default: break;
    }

    return flags;
}

inline uint32_t Board::evalInvalidationMaskFromMoveFlags(uint32_t moveFlags) noexcept {
    uint32_t mask = 0;
    const bool captureOrPromotion = (moveFlags & (MOVE_CHANGE_CAPTURE | MOVE_CHANGE_PROMOTION)) != 0;
    const bool pawnRelated = ((moveFlags & MOVE_CHANGE_PAWN_MOVE) != 0) || captureOrPromotion;

    if (captureOrPromotion) {
        mask |= evalCacheBit(EVAL_CACHE_MATERIAL_DELTA);
        mask |= evalCacheBit(EVAL_CACHE_BISHOP_PAIR_BONUS);
    }

    if (pawnRelated) {
        mask |= evalCacheBit(EVAL_CACHE_PAWN_STRUCTURE_MG);
        mask |= evalCacheBit(EVAL_CACHE_PAWN_STRUCTURE_EG);
        mask |= evalCacheBit(EVAL_CACHE_CENTRAL_CONTROL);
        mask |= evalCacheBit(EVAL_CACHE_BAD_BISHOP);
    }

    if (((moveFlags & MOVE_CHANGE_ROOK_MOVE) != 0) || pawnRelated) {
        mask |= evalCacheBit(EVAL_CACHE_ROOKS);
    }

    if (((moveFlags & MOVE_CHANGE_KING_MOVE) != 0)
        || ((moveFlags & MOVE_CHANGE_ROOK_MOVE) != 0)
        || ((moveFlags & MOVE_CHANGE_CASTLING) != 0)
        || ((moveFlags & MOVE_CHANGE_CAPTURE) != 0)) {
        mask |= evalCacheBit(EVAL_CACHE_CASTLING_BONUS);
    }

    if (((moveFlags & (MOVE_CHANGE_KNIGHT_MOVE | MOVE_CHANGE_BISHOP_MOVE)) != 0) || captureOrPromotion) {
        mask |= evalCacheBit(EVAL_CACHE_MINOR_DEVELOPMENT);
        mask |= evalCacheBit(EVAL_CACHE_OUTPOSTS);
    }
    if (pawnRelated) {
        mask |= evalCacheBit(EVAL_CACHE_OUTPOSTS);
    }

    if (((moveFlags & MOVE_CHANGE_QUEEN_MOVE) != 0) || captureOrPromotion) {
        mask |= evalCacheBit(EVAL_CACHE_EARLY_QUEEN);
    }

    if (((moveFlags & (MOVE_CHANGE_PAWN_MOVE
                     | MOVE_CHANGE_KNIGHT_MOVE
                     | MOVE_CHANGE_BISHOP_MOVE
                     | MOVE_CHANGE_ROOK_MOVE
                     | MOVE_CHANGE_QUEEN_MOVE)) != 0)
        || captureOrPromotion) {
        mask |= evalCacheBit(EVAL_CACHE_PIECE_COORDINATION);
    }

    return mask;
}

inline Board::MoveKind Board::classifyMoveKind(
    uint8_t movingType,
    uint8_t movingColor,
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t destBefore,
    const Coords& prevEnPassant
) noexcept {
    if (movingType == KING) {
        if ((fromIndex >> 3) == (toIndex >> 3)) {
            const int8_t df = static_cast<int8_t>((toIndex & 7) - (fromIndex & 7));
            if (df == 2 || df == -2) {
                return MoveKind::Castling;
            }
        }
        return (destBefore != EMPTY) ? MoveKind::Capture : MoveKind::Quiet;
    }

    if (movingType != PAWN) {
        return (destBefore != EMPTY) ? MoveKind::Capture : MoveKind::Quiet;
    }

    const uint8_t fromFile = fromIndex & 7;
    const uint8_t toFile = toIndex & 7;
    if (fromFile != toFile
        && destBefore == EMPTY
        && Coords::isInBounds(prevEnPassant)
        && toIndex == prevEnPassant.index) {
        return MoveKind::EnPassant;
    }

    const uint8_t toRank = toIndex >> 3;
    if (toRank == promotionRank(movingColor == WHITE)) {
        return (destBefore != EMPTY) ? MoveKind::PromotionCapture : MoveKind::PromotionQuiet;
    }

    const int8_t dr = static_cast<int8_t>((toIndex >> 3) - (fromIndex >> 3));
    if (dr == 2 || dr == -2) {
        return MoveKind::DoublePawnPush;
    }

    return (destBefore != EMPTY) ? MoveKind::Capture : MoveKind::Quiet;
}

__attribute__((always_inline))
inline uint8_t Board::normalizePromotionChoice(char promotionChoice) noexcept {
    uint8_t promo = static_cast<uint8_t>(promotionChoice);
    if (promo >= 'A' && promo <= 'Z') {
        promo = static_cast<uint8_t>(promo | 0x20);
    }

    if (promo == 'q' || promo == 'r' || promo == 'b' || promo == 'n') [[likely]] {
        return promo;
    }
    return static_cast<uint8_t>('q');
}

__attribute__((always_inline))
inline uint8_t Board::promotedPieceFromChoice(uint8_t promo, uint8_t movingColor) noexcept {
    if (promo == 'r') return ROOK | movingColor;
    if (promo == 'b') return BISHOP | movingColor;
    if (promo == 'n') return KNIGHT | movingColor;
    return QUEEN | movingColor;
}

// This function assumes the caller has already validated that the piece being promoted is a pawn
// and that the promotion choice is valid.
__attribute__((always_inline))
inline void Board::promoteUnchecked(uint8_t atIndex, uint8_t pawnPiece, uint8_t promo) noexcept {
    const uint8_t movingColor = pawnPiece & MASK_COLOR;
    const uint8_t newPiece = promotedPieceFromChoice(promo, movingColor);
    removePieceFromBB(pawnPiece, atIndex);
    addPieceToBB(newPiece, atIndex);
    set(atIndex, static_cast<piece_id>(newPiece));
}

__attribute__((always_inline))
inline void Board::snapshotState(MoveState& st) const noexcept {
    st.prevActiveColor   = activeColor;
    st.prevHalfMoveClock = halfMoveClock;
    st.prevFullMoveClock = fullMoveClock;
    st.prevEnPassant     = enPassant;
    st.prevEpHashFile    = epHashFile;
    st.prevCastle        = castle;
    st.prevHasMoved      = hasMoved;
    st.prevHistorySize   = historySize;
    st.prevHistoryHead   = currentHash;
    st.prevEvalCache     = evalCache;
    st.prevLastMoveChangeFlags = lastMoveChangeFlags;
}

__attribute__((always_inline))
inline void Board::prepareMoveState(MoveState& st, uint8_t moving, uint8_t destBefore) const noexcept {
    snapshotState(st);
    st.capturedPiece = destBefore;
    st.fromPiece = moving;
    st.promotionPieceType = 0;
    st.wasEnPassantCapture = false;
    st.enPassantCapturedIndex = 0;
    st.wasCastling = false;
    st.rookFromIndex = 0;
    st.rookToIndex = 0;
    st.historyWasReset = false;
}

__attribute__((always_inline))
inline void Board::prepareNullMoveState(MoveState& st) const noexcept {
    snapshotState(st);
    st.moveKind = MoveKind::Quiet;
    st.capturedPiece = EMPTY;
    st.fromPiece = EMPTY;
    st.promotionPieceType = 0;
    st.wasEnPassantCapture = false;
    st.enPassantCapturedIndex = 0;
    st.wasCastling = false;
    st.rookFromIndex = 0;
    st.rookToIndex = 0;
    st.historyWasReset = false;
}

inline void Board::applyEvalCacheInvalidation(const MoveState& st) noexcept {
    lastMoveChangeFlags = computeMoveChangeFlags(st);
    invalidateEvalCacheTerms(evalInvalidationMaskFromMoveFlags(lastMoveChangeFlags));
}

__attribute__((always_inline))
inline void Board::restoreState(const MoveState& st) noexcept {
    activeColor   = st.prevActiveColor;
    halfMoveClock = st.prevHalfMoveClock;
    fullMoveClock = st.prevFullMoveClock;
    enPassant     = st.prevEnPassant;
    epHashFile    = st.prevEpHashFile;
    castle        = st.prevCastle;
    hasMoved      = st.prevHasMoved;
    historySize   = st.prevHistorySize;
    currentHash   = st.prevHistoryHead;
    evalCache     = st.prevEvalCache;
    lastMoveChangeFlags = st.prevLastMoveChangeFlags;
}

__attribute__((always_inline))
inline uint8_t Board::rookStartSlot(uint8_t index) noexcept {
    switch (index) {
        case WHITE_ROOK_A_START: return 0;
        case WHITE_ROOK_H_START: return 1;
        case BLACK_ROOK_A_START: return 2;
        case BLACK_ROOK_H_START: return 3;
        default: return 0xFF;
    }
}

inline void Board::clearCastlingByRookStart(uint8_t rookStartIndex, bool setHasMovedBit) noexcept {
    static constexpr std::array<uint8_t, 4> ROOK_CASTLE_CLEAR_MASKS = {
        static_cast<uint8_t>(1u << WHITE_QUEENSIDE),
        static_cast<uint8_t>(1u << WHITE_KINGSIDE),
        static_cast<uint8_t>(1u << BLACK_QUEENSIDE),
        static_cast<uint8_t>(1u << BLACK_KINGSIDE)
    };
    static constexpr std::array<uint8_t, 4> ROOK_HAS_MOVED_BITS = {
        static_cast<uint8_t>(1u << 1),
        static_cast<uint8_t>(1u << 2),
        static_cast<uint8_t>(1u << 4),
        static_cast<uint8_t>(1u << 5)
    };

    const uint8_t slot = rookStartSlot(rookStartIndex);
    if (slot == 0xFF) return;

    castle &= static_cast<uint8_t>(~ROOK_CASTLE_CLEAR_MASKS[slot]);
    if (setHasMovedBit) {
        hasMoved |= ROOK_HAS_MOVED_BITS[slot];
    }
}

inline void Board::updateCastlingRightsOnPieceMove(uint8_t movingType, uint8_t movingColor, uint8_t fromIndex) noexcept {
    if (movingType == KING) {
        const uint8_t kingBit = (movingColor == WHITE) ? 0x01 : 0x08;
        const uint8_t castleMask = (movingColor == WHITE) ? 0x03 : 0x0C;
        castle &= static_cast<uint8_t>(~castleMask);
        hasMoved |= kingBit;
        return;
    }

    if (movingType == ROOK) {
        clearCastlingByRookStart(fromIndex, true);
    }
}

inline void Board::updateCastlingRightsOnRookCapture(uint8_t capturedPiece, uint8_t toIndex) noexcept {
    if ((capturedPiece & MASK_PIECE_TYPE) != ROOK) return;

    const uint8_t slot = rookStartSlot(toIndex);
    if (slot == 0xFF) return;

    const uint8_t capturedColor = capturedPiece & MASK_COLOR;
    if ((capturedColor == WHITE && slot >= 2) || (capturedColor == BLACK && slot < 2)) return;

    clearCastlingByRookStart(toIndex, false);
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
        const uint8_t fromFile = static_cast<uint8_t>(fromIndex & 7);
        const uint8_t toFile = static_cast<uint8_t>(toIndex & 7);
        const uint8_t rankBase = static_cast<uint8_t>(toIndex & 56);
        const uint8_t rookFromFile = (toFile > fromFile) ? 7 : 0;
        const uint8_t rookToFile   = (toFile > fromFile) ? 5 : 3;
        const uint8_t rookFromIndex = static_cast<uint8_t>(rankBase | rookFromFile);
        const uint8_t rookToIndex   = static_cast<uint8_t>(rankBase | rookToFile);
        st.rookFromIndex = rookFromIndex;
        st.rookToIndex   = rookToIndex;

        const uint8_t rook = get(rookFromIndex);
        set(rookToIndex, static_cast<piece_id>(rook));
        set(rookFromIndex, EMPTY);
        fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
        removePieceFromBB(rook, rookFromIndex);
        addPieceToBB(rook, rookToIndex);
    }

    updateCastlingRightsOnPieceMove(movingType, movingColor, fromIndex);

    if constexpr (isCaptureKind(Kind)) {
        updateCastlingRightsOnRookCapture(destBefore, toIndex);
    }

    if constexpr (Kind == MoveKind::DoublePawnPush) {
        const uint8_t enPassantIndex = static_cast<uint8_t>((fromIndex + toIndex) >> 1);
        enPassant = Coords{enPassantIndex};
    }

    if constexpr (isPromotionKind(Kind)) {
        const uint8_t promo = normalizePromotionChoice(promotionChoice);
        st.promotionPieceType = promo;
        promoteUnchecked(toIndex, moving, promo);
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
