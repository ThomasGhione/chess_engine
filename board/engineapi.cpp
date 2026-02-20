#include "board.hpp"
#include "../tt/zobrist.hpp"
#include <cstring>


namespace chess {



// ------------------------------------------------------------
// INCREMENTAL DO/UNDO MOVE (NO LEGALITY CHECKS)
// ------------------------------------------------------------

void Board::doMove(const Move& m, MoveState& st, char promotionChoice) noexcept {
    const Coords& from = m.from;
    const Coords& to   = m.to;

    const uint8_t fromIndex = from.toIndex();
    const uint8_t toIndex   = to.toIndex();
    
    const uint8_t fromFile = fromIndex & 7;
    const uint8_t fromRank = fromIndex >> 3;
    const uint8_t toFile = toIndex & 7;
    const uint8_t toRank = toIndex >> 3;

    const uint8_t moving      = get(from);
    const uint8_t movingType  = moving & MASK_PIECE_TYPE;
    const uint8_t movingColor = moving & MASK_COLOR;
    const uint8_t destBefore  = get(to);

    std::memset(static_cast<void*>(&st), 0, sizeof(st));
    st.prevActiveColor   = activeColor;
    st.prevHalfMoveClock = halfMoveClock;
    st.prevFullMoveClock = fullMoveClock;
    st.prevEnPassant     = enPassant;
    st.prevCastle        = castle;
    st.prevHasMoved      = hasMoved;
    st.capturedPiece     = destBefore;
    st.fromPiece         = moving;
    st.prevHistorySize   = historySize;
    st.prevHistoryHead   = currentHash;

    uint64_t newHash = currentHash;
    if (Coords::isInBounds(st.prevEnPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, st.prevEnPassant)) {
        newHash ^= zobrist::TABLES.enPassant[st.prevEnPassant.file()];
    }
    newHash ^= zobrist::TABLES.pieces[moving][fromIndex];

    enPassant = Coords{};

    // --- EN PASSANT CAPTURE ---
    if (movingType == PAWN) {
        if (fromFile != toFile && destBefore == EMPTY &&
            Coords::isInBounds(st.prevEnPassant) &&
            toIndex == st.prevEnPassant.toIndex()) {

            st.wasEnPassantCapture = true;

            const bool isWhite = (movingColor == WHITE);
            // Coords convention: white moves toward rank 0, black toward rank 7
            const int8_t forwardDir = isWhite ? -1 : 1;
            Coords captured{toFile, static_cast<uint8_t>(toRank - forwardDir)};
            const uint8_t capturedPiece = get(captured);
            st.capturedPiece = capturedPiece;

            const uint8_t capIndex = captured.toIndex();
            st.enPassantCapturedIndex = capIndex;

            set(captured, EMPTY);
            occupancy &= ~(bitMask(capIndex));
            removePieceFromBB(capturedPiece, capIndex);
            newHash ^= zobrist::TABLES.pieces[capturedPiece][capIndex];
        }
    }

    // --- NORMAL CAPTURE SU DESTINAZIONE ---
    if (destBefore != EMPTY && !st.wasEnPassantCapture) {
        removePieceFromBB(destBefore, toIndex);
        newHash ^= zobrist::TABLES.pieces[destBefore][toIndex];
    }

    // --- SPOSTAMENTO PEZZO ---
    updateChessboard(from, to, static_cast<piece_id>(moving));
    fastUpdateOccupancyBB(fromIndex, toIndex);
    removePieceFromBB(moving, fromIndex);
    addPieceToBB(moving, toIndex);
    newHash ^= zobrist::TABLES.pieces[moving][toIndex];

    // --- ARROCCO: spostamento torre ---
    if (movingType == KING && fromRank == toRank) {
        const int8_t df = static_cast<int8_t>(toFile - fromFile);
        if (df == 2 || df == -2) {
            st.wasCastling = true;

            // Compute rook indices directly
            const uint8_t rookFromFile = (df == 2) ? 7 : 0;
            const uint8_t rookToFile   = (df == 2) ? 5 : 3;
            const uint8_t rookFromIndex = (toRank << 3) | rookFromFile;
            const uint8_t rookToIndex   = (toRank << 3) | rookToFile;

            st.rookFromIndex = rookFromIndex;
            st.rookToIndex   = rookToIndex;

            // Use index-based get/set for efficiency (get(index) does Coords conversion internally)
            const uint8_t rook = get(rookFromIndex);
            set(rookToIndex, static_cast<piece_id>(rook));
            set(rookFromIndex, EMPTY);
            fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
            removePieceFromBB(rook, rookFromIndex);
            addPieceToBB(rook, rookToIndex);
            newHash ^= zobrist::TABLES.pieces[rook][rookFromIndex];
            newHash ^= zobrist::TABLES.pieces[rook][rookToIndex];
        }
    }

    // --- UPDATE DIRITTI DI ARROCCO / hasMoved ---
    if (movingType == KING) {
        const uint8_t kingBit = (movingColor == WHITE) ? 0x01 : 0x08;  // bit 0 or bit 3
        const uint8_t castleMask = (movingColor == WHITE) ? 0x03 : 0x0C;  // bits 0-1 or bits 2-3
        castle &= ~castleMask;
        hasMoved |= kingBit;
    } else if (movingType == ROOK) {
        // White rooks at rank 7 (row 1), Black rooks at rank 0 (row 8)
        const bool isInitialSquare = (movingColor == WHITE)
            ? (fromRank == 7 && (fromFile == 0 || fromFile == 7))
            : (fromRank == 0 && (fromFile == 0 || fromFile == 7));
        
        if (isInitialSquare) {
            if (movingColor == WHITE) {
                if (fromFile == 0) {
                    castle &= ~(1u << 1); // white queenside
                    hasMoved |= (1u << 1);
                } else {
                    castle &= ~(1u << 0); // white kingside
                    hasMoved |= (1u << 2);
                }
            } else {
                if (fromFile == 0) {
                    castle &= ~(1u << 3); // black queenside
                    hasMoved |= (1u << 4);
                } else {
                    castle &= ~(1u << 2); // black kingside
                    hasMoved |= (1u << 5);
                }
            }
        }
    }
    
    // Captured rook on initial square disables corresponding castling
    if (destBefore != EMPTY && (destBefore & MASK_PIECE_TYPE) == ROOK) {
        // White rooks at rank 7 (row 1), Black rooks at rank 0 (row 8)
        const bool isInitialSquare = ((destBefore & MASK_COLOR) == WHITE)
            ? (toRank == 7 && (toFile == 0 || toFile == 7))
            : (toRank == 0 && (toFile == 0 || toFile == 7));
        
        if (isInitialSquare) {
            if ((destBefore & MASK_COLOR) == WHITE) {
                castle &= (toFile == 0) 
                    ? ~(1u << 1)  // white queenside
                    : ~(1u << 0); // white kingside
            } else {
                castle &= (toFile == 0)
                    ? ~(1u << 3)  // black queenside
                    : ~(1u << 2); // black kingside
            }
        }
    }

    if (movingType == PAWN) {
        // --- EN PASSANT TARGET DOPO DOPPIO PASSO ---
        const int8_t dr = static_cast<int8_t>(toRank - fromRank);
        if (dr == 2 || dr == -2) {
            enPassant = Coords{fromFile, static_cast<uint8_t>((fromRank + toRank) >> 1)};
        }

        // --- PROMOZIONE ---
        const bool isWhiteMoving = (movingColor == WHITE);
        const uint8_t promoRank = Board::promotionRank(isWhiteMoving);
        if (static_cast<uint8_t>(toRank) == promoRank) {
            uint8_t promo = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(promotionChoice)));
            if (promo != 'q' && promo != 'r' && promo != 'b' && promo != 'n') {
                promo = 'q';
            }
            st.promotionPieceType = promo;
            (void)promote(to, static_cast<char>(promo));
            uint8_t promotedPiece = QUEEN | movingColor;
            if (promo == 'r') promotedPiece = ROOK | movingColor;
            else if (promo == 'b') promotedPiece = BISHOP | movingColor;
            else if (promo == 'n') promotedPiece = KNIGHT | movingColor;
            newHash ^= zobrist::TABLES.pieces[moving][toIndex];
            newHash ^= zobrist::TABLES.pieces[promotedPiece][toIndex];
        }
        
    }

    // --- CLOCKS E SIDE TO MOVE ---
    const bool resetHistory = (movingType == PAWN || destBefore != EMPTY || st.wasEnPassantCapture);
    st.historyWasReset = resetHistory;
    if (resetHistory) {
        halfMoveClock = 0;
    } else if (halfMoveClock < 255) {
        ++halfMoveClock;
    }
    if (activeColor == BLACK && fullMoveClock < 255) {
        ++fullMoveClock;
    }
    activeColor = oppositeColor(activeColor);
    newHash ^= zobrist::TABLES.sideToMove;

    if (castle != st.prevCastle) {
        newHash ^= zobrist::TABLES.castling[st.prevCastle];
        newHash ^= zobrist::TABLES.castling[castle];
    }
    if (Coords::isInBounds(enPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, enPassant)) {
        newHash ^= zobrist::TABLES.enPassant[enPassant.file()];
    }
    currentHash = newHash;

    updateRepetitionAfterMove(resetHistory, false);
}

void Board::undoMove(const Move& m, const MoveState& st) noexcept {
    const Coords& from = m.from;
    const Coords& to   = m.to;

    const uint8_t fromIndex = from.toIndex();
    const uint8_t toIndex   = to.toIndex();

    uint8_t pieceOnTo = get(toIndex);  // usa index-based
    uint8_t pieceType = pieceOnTo & MASK_PIECE_TYPE;

    // --- ANNULLA PROMOZIONE: pezzo promosso torna pedone ---
    if (st.promotionPieceType != 0 && pieceType != PAWN) {
        const uint8_t color = pieceOnTo & MASK_COLOR;
        const uint8_t pawnPiece = (PAWN | color);
        removePieceFromBB(pieceOnTo, toIndex);
        addPieceToBB(pawnPiece, toIndex);
        set(toIndex, static_cast<piece_id>(pawnPiece));  // usa index-based
        pieceOnTo = pawnPiece;
        // pieceType = PAWN; // non necessario, non usato dopo
    }

    // --- SPOSTA IL PEZZO INDIETRO (to -> from) ---
    updateChessboard(to, from, static_cast<piece_id>(pieceOnTo));
    fastUpdateOccupancyBB(toIndex, fromIndex);
    removePieceFromBB(pieceOnTo, toIndex);
    addPieceToBB(pieceOnTo, fromIndex);

    // --- RIPRISTINA PEZZO CATTURATO (EP o normale) ---
    if (st.wasEnPassantCapture) {
        const uint8_t capIndex = st.enPassantCapturedIndex;
        // Use index-based set for efficiency
        set(capIndex, static_cast<piece_id>(st.capturedPiece));
        occupancy |= bitMask(capIndex);
        addPieceToBB(st.capturedPiece, capIndex);
    } else if (st.capturedPiece != EMPTY) {
        set(toIndex, static_cast<piece_id>(st.capturedPiece));  // usa index-based
        occupancy |= bitMask(toIndex);
        addPieceToBB(st.capturedPiece, toIndex);
    }

    // --- ANNULLA SPOSTAMENTO TORRE IN ARROCCO ---
    if (st.wasCastling) {
        const uint8_t rookFromIndex = st.rookFromIndex;
        const uint8_t rookToIndex   = st.rookToIndex;
        
        // Use index-based operations for efficiency
        const uint8_t rook = get(rookToIndex);
        set(rookFromIndex, static_cast<piece_id>(rook));
        set(rookToIndex, EMPTY);
        fastUpdateOccupancyBB(rookToIndex, rookFromIndex);
        removePieceFromBB(rook, rookToIndex);
        addPieceToBB(rook, rookFromIndex);
    }

    // --- RIPRISTINA GAME STATE ---
    activeColor   = st.prevActiveColor;
    halfMoveClock = st.prevHalfMoveClock;
    fullMoveClock = st.prevFullMoveClock;
    enPassant     = st.prevEnPassant;
    castle        = st.prevCastle;
    hasMoved      = st.prevHasMoved;

    // FIX: Restore repetition history correctly
    // Simply restore the saved state - no need to recompute hash
    historySize = st.prevHistorySize;
    currentHash = st.prevHistoryHead;
}

void Board::doNullMove(MoveState& st) noexcept {
    st.prevActiveColor   = activeColor;
    st.prevHalfMoveClock = halfMoveClock;
    st.prevFullMoveClock = fullMoveClock;
    st.prevEnPassant     = enPassant;
    st.prevCastle        = castle;
    st.prevHasMoved      = hasMoved;
    st.prevHistorySize   = historySize;
    st.prevHistoryHead   = currentHash;
    uint64_t newHash = currentHash;
    if (Coords::isInBounds(st.prevEnPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, st.prevEnPassant)) {
        newHash ^= zobrist::TABLES.enPassant[st.prevEnPassant.file()];
    }

    // Null move clears en-passant rights and just passes the turn.
    enPassant = Coords{};

    if (halfMoveClock < 255) {
        ++halfMoveClock;
    }
    if (activeColor == BLACK && fullMoveClock < 255) {
        ++fullMoveClock;
    }
    activeColor = oppositeColor(activeColor);
    newHash ^= zobrist::TABLES.sideToMove;
    currentHash = newHash;

    // Keep hash/repetition coherent with the null-move position.
    updateRepetitionAfterMove(false, false);
}

void Board::undoNullMove(const MoveState& st) noexcept {
    activeColor   = st.prevActiveColor;
    halfMoveClock = st.prevHalfMoveClock;
    fullMoveClock = st.prevFullMoveClock;
    enPassant     = st.prevEnPassant;
    castle        = st.prevCastle;
    hasMoved      = st.prevHasMoved;
    historySize   = st.prevHistorySize;
    currentHash   = st.prevHistoryHead;
}


}
