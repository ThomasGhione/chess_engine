#include "board.hpp"


namespace chess {



// ------------------------------------------------------------
// INCREMENTAL DO/UNDO MOVE (NO LEGALITY CHECKS)
// ------------------------------------------------------------

void Board::doMove(const Move& m, MoveState& st, char promotionChoice) noexcept {
    const Coords& from = m.from;
    const Coords& to   = m.to;

    const uint8_t fromIndex = from.toIndex();
    const uint8_t toIndex   = to.toIndex();

    const uint8_t moving      = this->get(from);
    const uint8_t movingType  = moving & MASK_PIECE_TYPE;
    const uint8_t movingColor = moving & MASK_COLOR;
    const uint8_t destBefore  = this->get(to);

    st = MoveState{
        .prevActiveColor         = activeColor,
        .prevHalfMoveClock       = halfMoveClock,
        .prevFullMoveClock       = fullMoveClock,
        .prevEnPassant           = {enPassant[0], enPassant[1]},
        .prevCastle              = castle,
        .prevHasMoved            = hasMoved,
        .capturedPiece           = destBefore,
        .fromPiece               = moving,
        .promotionPieceType      = 0,
        .wasEnPassantCapture     = false,
        .enPassantCapturedIndex  = 0,
        .wasCastling             = false,
        .rookFromIndex           = 0,
        .rookToIndex             = 0
    };

    // Cache opzionale re
    //st.prevWhiteKingIndex = kings_bb[0] ? __builtin_ctzll(kings_bb[0]) : 64;
    //st.prevBlackKingIndex = kings_bb[1] ? __builtin_ctzll(kings_bb[1]) : 64;

    // Reset en passant di default (potrà essere reimpostato per un doppio passo)
    enPassant[0] = Coords{};
    enPassant[1] = Coords{};

    // --- EN PASSANT CAPTURE ---
    if (movingType == PAWN) {
        if (from.file() != to.file() && destBefore == EMPTY &&
            Coords::isInBounds(st.prevEnPassant[0]) &&
            toIndex == st.prevEnPassant[0].toIndex()) {

            st.wasEnPassantCapture = true;

            const bool isWhite = (movingColor == WHITE);
            // Coords convention: white moves toward rank 0, black toward rank 7
            const int8_t forwardDir = isWhite ? -1 : 1;
            Coords captured{to.file(), static_cast<uint8_t>(to.rank() - forwardDir)};
            const uint8_t capturedPiece = this->get(captured);
            st.capturedPiece = capturedPiece;

            const uint8_t capIndex = captured.toIndex();
            st.enPassantCapturedIndex = capIndex;

            this->set(captured, EMPTY);
            this->occupancy &= ~(1ULL << capIndex);
            this->removePieceFromBitboards(capturedPiece, capIndex);
        }
    }

    // --- NORMAL CAPTURE SU DESTINAZIONE ---
    if (destBefore != EMPTY && !st.wasEnPassantCapture) {
        this->removePieceFromBitboards(destBefore, toIndex);
    }

    // --- SPOSTAMENTO PEZZO ---
    this->updateChessboard(from, to);
    this->fastUpdateOccupancyBB(fromIndex, toIndex);
    this->removePieceFromBitboards(moving, fromIndex);
    this->addPieceToBitboards(moving, toIndex);

    // --- ARROCCO: spostamento torre ---
    if (movingType == KING && from.rank() == to.rank()) {
        const int8_t df = static_cast<int8_t>(to.file() - from.file());
        if (df == 2 || df == -2) {
            st.wasCastling = true;

            // Compute rook indices directly
            const uint8_t rookFromFile = (df == 2) ? 7 : 0;
            const uint8_t rookToFile   = (df == 2) ? 5 : 3;
            const uint8_t rookFromIndex = (to.rank() << 3) | rookFromFile;
            const uint8_t rookToIndex   = (to.rank() << 3) | rookToFile;

            st.rookFromIndex = rookFromIndex;
            st.rookToIndex   = rookToIndex;

            // Use index-based get/set for efficiency (get(index) does Coords conversion internally)
            const uint8_t rook = this->get(rookFromIndex);
            this->set(rookToIndex, static_cast<piece_id>(rook));
            this->set(rookFromIndex, EMPTY);
            this->fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
            this->removePieceFromBitboards(rook, rookFromIndex);
            this->addPieceToBitboards(rook, rookToIndex);
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
            ? (from.rank() == 7 && (from.file() == 0 || from.file() == 7))
            : (from.rank() == 0 && (from.file() == 0 || from.file() == 7));
        
        if (isInitialSquare) {
            if (movingColor == WHITE) {
                if (from.file() == 0) {
                    castle &= ~(1u << 1); // white queenside
                    hasMoved |= (1u << 1);
                } else {
                    castle &= ~(1u << 0); // white kingside
                    hasMoved |= (1u << 2);
                }
            } else {
                if (from.file() == 0) {
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
            ? (to.rank() == 7 && (to.file() == 0 || to.file() == 7))
            : (to.rank() == 0 && (to.file() == 0 || to.file() == 7));
        
        if (isInitialSquare) {
            if ((destBefore & MASK_COLOR) == WHITE) {
                castle &= (to.file() == 0) 
                    ? ~(1u << 1)  // white queenside
                    : ~(1u << 0); // white kingside
            } else {
                castle &= (to.file() == 0)
                    ? ~(1u << 3)  // black queenside
                    : ~(1u << 2); // black kingside
            }
        }
    }

    // --- EN PASSANT TARGET DOPO DOPPIO PASSO ---
    if (movingType == PAWN) {
        const int8_t dr = static_cast<int8_t>(to.rank() - from.rank());
        if (dr == 2 || dr == -2) {
            enPassant[0] = Coords{from.file(), static_cast<uint8_t>((from.rank() + to.rank()) >> 1)};
        }
    }

    // --- PROMOZIONE ---
    if (movingType == PAWN) {
        // White promotes at rank 0 (row 8), Black promotes at rank 7 (row 1)
        if ((movingColor == WHITE && to.rank() == 0) ||
            (movingColor == BLACK && to.rank() == 7)) {

            uint8_t promo = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(promotionChoice)));
            if (promo != 'q' && promo != 'r' && promo != 'b' && promo != 'n') {
                promo = 'q';
            }
            st.promotionPieceType = promo;
            (void)this->promote(to, static_cast<char>(promo));
        }
    }

    // --- CLOCKS E SIDE TO MOVE ---
    if (movingType == PAWN || destBefore != EMPTY || st.wasEnPassantCapture) {
        halfMoveClock = 0;
    } else if (halfMoveClock < 255) {
        ++halfMoveClock;
    }
    if (activeColor == BLACK && fullMoveClock < 255) {
        ++fullMoveClock;
    }
    activeColor = (activeColor == WHITE) ? BLACK : WHITE;
}

void Board::undoMove(const Move& m, const MoveState& st) noexcept {
    const Coords& from = m.from;
    const Coords& to   = m.to;

    const uint8_t fromIndex = from.toIndex();
    const uint8_t toIndex   = to.toIndex();

    uint8_t pieceOnTo = this->get(to);
    uint8_t pieceType = pieceOnTo & MASK_PIECE_TYPE;

    // --- ANNULLA PROMOZIONE: pezzo promosso torna pedone ---
    if (st.promotionPieceType != 0 && pieceType != PAWN) {
        const uint8_t color = pieceOnTo & MASK_COLOR;
        const uint8_t pawnPiece = (PAWN | color);
        this->removePieceFromBitboards(pieceOnTo, toIndex);
        this->addPieceToBitboards(pawnPiece, toIndex);
        this->set(to, static_cast<piece_id>(pawnPiece));
        pieceOnTo = pawnPiece;
        pieceType = PAWN;
    }

    // --- SPOSTA IL PEZZO INDIETRO (to -> from) ---
    this->updateChessboard(to, from);
    this->fastUpdateOccupancyBB(toIndex, fromIndex);
    this->removePieceFromBitboards(pieceOnTo, toIndex);
    this->addPieceToBitboards(pieceOnTo, fromIndex);

    // --- RIPRISTINA PEZZO CATTURATO (EP o normale) ---
    if (st.wasEnPassantCapture) {
        const uint8_t capIndex = st.enPassantCapturedIndex;
        // Use index-based set for efficiency
        this->set(capIndex, static_cast<piece_id>(st.capturedPiece));
        this->occupancy |= (1ULL << capIndex);
        this->addPieceToBitboards(st.capturedPiece, capIndex);
    } else if (st.capturedPiece != EMPTY) {
        this->set(to, static_cast<piece_id>(st.capturedPiece));
        this->occupancy |= (1ULL << toIndex);
        this->addPieceToBitboards(st.capturedPiece, toIndex);
    }

    // --- ANNULLA SPOSTAMENTO TORRE IN ARROCCO ---
    if (st.wasCastling) {
        const uint8_t rookFromIndex = st.rookFromIndex;
        const uint8_t rookToIndex   = st.rookToIndex;
        
        // Use index-based operations for efficiency
        const uint8_t rook = this->get(rookToIndex);
        this->set(rookFromIndex, static_cast<piece_id>(rook));
        this->set(rookToIndex, EMPTY);
        this->fastUpdateOccupancyBB(rookToIndex, rookFromIndex);
        this->removePieceFromBitboards(rook, rookToIndex);
        this->addPieceToBitboards(rook, rookFromIndex);
    }

    // --- RIPRISTINA GAME STATE ---
    activeColor   = st.prevActiveColor;
    halfMoveClock = st.prevHalfMoveClock;
    fullMoveClock = st.prevFullMoveClock;
    enPassant[0]  = st.prevEnPassant[0];
    enPassant[1]  = st.prevEnPassant[1];
    castle        = st.prevCastle;
    hasMoved      = st.prevHasMoved;
}


}