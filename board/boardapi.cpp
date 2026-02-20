#include "board.hpp"
#include "../tt/zobrist.hpp"
#include <cstring>


namespace chess {


// ------------------------------------------------------------
// INCREMENTAL DO/UNDO MOVE (NO LEGALITY CHECKS)
// ------------------------------------------------------------
__attribute__((hot))
void Board::doMove(const Move& m, MoveState& st, char promotionChoice) noexcept {
    const Coords& from = m.from;
    const Coords& to   = m.to;

    const uint8_t fromIndex = from.index;
    const uint8_t toIndex   = to.index;
    
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
    st.moveKind          = classifyMoveKind(movingType, movingColor, fromIndex, toIndex, destBefore, st.prevEnPassant);

    uint64_t newHash = currentHash;
    if (Coords::isInBounds(st.prevEnPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, st.prevEnPassant)) {
        newHash ^= zobrist::TABLES.enPassant[st.prevEnPassant.file()];
    }
    newHash ^= zobrist::TABLES.pieces[moving][fromIndex];

    enPassant = Coords{};

    const MoveKind kind = st.moveKind;
    switch (kind) {
        case MoveKind::Capture:
            doMoveByKind<MoveKind::Capture>(from, to, st, moving, movingType, movingColor, destBefore,
                                            fromIndex, toIndex, fromFile, fromRank, toFile, toRank, promotionChoice);
            break;
        case MoveKind::DoublePawnPush:
            doMoveByKind<MoveKind::DoublePawnPush>(from, to, st, moving, movingType, movingColor, destBefore,
                                                   fromIndex, toIndex, fromFile, fromRank, toFile, toRank, promotionChoice);
            break;
        case MoveKind::EnPassant:
            doMoveByKind<MoveKind::EnPassant>(from, to, st, moving, movingType, movingColor, destBefore,
                                              fromIndex, toIndex, fromFile, fromRank, toFile, toRank, promotionChoice);
            newHash ^= zobrist::TABLES.pieces[st.capturedPiece][st.enPassantCapturedIndex];
            break;
        case MoveKind::Castling:
            doMoveByKind<MoveKind::Castling>(from, to, st, moving, movingType, movingColor, destBefore,
                                             fromIndex, toIndex, fromFile, fromRank, toFile, toRank, promotionChoice);
            {
                const uint8_t rook = get(st.rookToIndex);
                newHash ^= zobrist::TABLES.pieces[rook][st.rookFromIndex];
                newHash ^= zobrist::TABLES.pieces[rook][st.rookToIndex];
            }
            break;
        case MoveKind::PromotionQuiet:
            doMoveByKind<MoveKind::PromotionQuiet>(from, to, st, moving, movingType, movingColor, destBefore,
                                                   fromIndex, toIndex, fromFile, fromRank, toFile, toRank, promotionChoice);
            {
                const uint8_t promotedPiece = promotedPieceFromChoice(st.promotionPieceType, movingColor);
                newHash ^= zobrist::TABLES.pieces[promotedPiece][toIndex];
            }
            break;
        case MoveKind::PromotionCapture:
            doMoveByKind<MoveKind::PromotionCapture>(from, to, st, moving, movingType, movingColor, destBefore,
                                                     fromIndex, toIndex, fromFile, fromRank, toFile, toRank, promotionChoice);
            newHash ^= zobrist::TABLES.pieces[destBefore][toIndex];
            {
                const uint8_t promotedPiece = promotedPieceFromChoice(st.promotionPieceType, movingColor);
                newHash ^= zobrist::TABLES.pieces[promotedPiece][toIndex];
            }
            break;
        case MoveKind::Quiet:
        default:
            doMoveByKind<MoveKind::Quiet>(from, to, st, moving, movingType, movingColor, destBefore,
                                          fromIndex, toIndex, fromFile, fromRank, toFile, toRank, promotionChoice);
            break;
    }
    newHash ^= zobrist::TABLES.pieces[moving][toIndex];

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

__attribute__((hot))
void Board::undoMove(const Move& m, const MoveState& st) noexcept {
    const Coords& from = m.from;
    const Coords& to   = m.to;

    const uint8_t fromIndex = from.index;
    const uint8_t toIndex   = to.index;

    uint8_t pieceOnTo = get(toIndex);  // usa index-based

    switch (st.moveKind) {
        case MoveKind::Capture:
            undoMoveByKind<MoveKind::Capture>(from, to, st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::DoublePawnPush:
            undoMoveByKind<MoveKind::DoublePawnPush>(from, to, st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::EnPassant:
            undoMoveByKind<MoveKind::EnPassant>(from, to, st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::Castling:
            undoMoveByKind<MoveKind::Castling>(from, to, st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::PromotionQuiet:
            undoMoveByKind<MoveKind::PromotionQuiet>(from, to, st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::PromotionCapture:
            undoMoveByKind<MoveKind::PromotionCapture>(from, to, st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::Quiet:
        default:
            undoMoveByKind<MoveKind::Quiet>(from, to, st, pieceOnTo, fromIndex, toIndex);
            break;
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

__attribute__((hot))
void Board::doNullMove(MoveState& st) noexcept {
    st.moveKind          = MoveKind::Quiet;
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

__attribute__((hot))
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
