#include "board.hpp"
#include "../tt/zobrist.hpp"


namespace chess {
// ------------------------------------------------------------
// INCREMENTAL DO/UNDO MOVE (NO LEGALITY CHECKS)
// ------------------------------------------------------------
__attribute__((hot))
void Board::doMove(const Move& m, MoveState& st, char promotionChoice) noexcept {
    //FIXME Eliminare costati magiche
    const uint8_t fromIndex = m.from.index;
    const uint8_t toIndex   = m.to.index;

    const uint8_t moving      = get(fromIndex);
    const uint8_t movingType  = moving & MASK_PIECE_TYPE;
    const uint8_t movingColor = moving & MASK_COLOR;
    const uint8_t destBefore  = get(toIndex);

    prepareMoveState(st, moving, destBefore);
    st.moveKind          = classifyMoveKind(movingType, movingColor, fromIndex, toIndex, destBefore, st.prevEnPassant);

    uint64_t newHash = currentHash;
    if (st.prevEpHashFile < 8) {
        newHash ^= zobrist::TABLES.enPassant[st.prevEpHashFile];
    }
    newHash ^= zobrist::TABLES.pieces[moving][fromIndex];

    enPassant = Coords{};

    const MoveKind kind = st.moveKind;
    switch (kind) {
        case MoveKind::Capture:
            doMoveByKind<MoveKind::Capture>(st, moving, movingType, movingColor, destBefore,
                                            fromIndex, toIndex, promotionChoice);
            if (destBefore != EMPTY) {
                newHash ^= zobrist::TABLES.pieces[destBefore][toIndex];
            }
            break;
        case MoveKind::DoublePawnPush:
            doMoveByKind<MoveKind::DoublePawnPush>(st, moving, movingType, movingColor, destBefore,
                                                   fromIndex, toIndex, promotionChoice);
            break;
        case MoveKind::EnPassant:
            doMoveByKind<MoveKind::EnPassant>(st, moving, movingType, movingColor, destBefore,
                                              fromIndex, toIndex, promotionChoice);
            newHash ^= zobrist::TABLES.pieces[st.capturedPiece][st.enPassantCapturedIndex];
            break;
        case MoveKind::Castling:
            doMoveByKind<MoveKind::Castling>(st, moving, movingType, movingColor, destBefore,
                                             fromIndex, toIndex, promotionChoice);
            {
                const uint8_t rook = get(st.rookToIndex);
                newHash ^= zobrist::TABLES.pieces[rook][st.rookFromIndex];
                newHash ^= zobrist::TABLES.pieces[rook][st.rookToIndex];
            }
            break;
        case MoveKind::PromotionQuiet:
            doMoveByKind<MoveKind::PromotionQuiet>(st, moving, movingType, movingColor, destBefore,
                                                   fromIndex, toIndex, promotionChoice);
            {
                const uint8_t promotedPiece = promotedPieceFromChoice(st.promotionPieceType, movingColor);
                newHash ^= zobrist::TABLES.pieces[promotedPiece][toIndex];
            }
            break;
        case MoveKind::PromotionCapture:
            doMoveByKind<MoveKind::PromotionCapture>(st, moving, movingType, movingColor, destBefore,
                                                     fromIndex, toIndex, promotionChoice);
            newHash ^= zobrist::TABLES.pieces[destBefore][toIndex];
            {
                const uint8_t promotedPiece = promotedPieceFromChoice(st.promotionPieceType, movingColor);
                newHash ^= zobrist::TABLES.pieces[promotedPiece][toIndex];
            }
            break;
        case MoveKind::Quiet:
        default:
            doMoveByKind<MoveKind::Quiet>(st, moving, movingType, movingColor, destBefore,
                                          fromIndex, toIndex, promotionChoice);
            break;
    }
    applyEvalCacheInvalidation(st);

    if (!isPromotionKind(kind)) {
        newHash ^= zobrist::TABLES.pieces[moving][toIndex];
    }

    const bool resetHistory = (movingType == PAWN || destBefore != EMPTY);
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
    uint8_t newEpHashFile = 0xFF;
    if (Coords::isInBounds(enPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, enPassant)) {
        newEpHashFile = enPassant.file();
        newHash ^= zobrist::TABLES.enPassant[newEpHashFile];
    }
    epHashFile = newEpHashFile;
    currentHash = newHash;

    updateRepetitionAfterMove(resetHistory, st);
}

__attribute__((hot))
void Board::undoMove(const Move& m, const MoveState& st) noexcept {
    const uint8_t fromIndex = m.from.index;
    const uint8_t toIndex   = m.to.index;

    uint8_t pieceOnTo = get(toIndex);  // use index-based access

    switch (st.moveKind) {
        case MoveKind::Capture:
            undoMoveByKind<MoveKind::Capture>(st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::DoublePawnPush:
            undoMoveByKind<MoveKind::DoublePawnPush>(st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::EnPassant:
            undoMoveByKind<MoveKind::EnPassant>(st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::Castling:
            undoMoveByKind<MoveKind::Castling>(st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::PromotionQuiet:
            undoMoveByKind<MoveKind::PromotionQuiet>(st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::PromotionCapture:
            undoMoveByKind<MoveKind::PromotionCapture>(st, pieceOnTo, fromIndex, toIndex);
            break;
        case MoveKind::Quiet:
        default:
            undoMoveByKind<MoveKind::Quiet>(st, pieceOnTo, fromIndex, toIndex);
            break;
    }

    restoreState(st);
}

__attribute__((hot))
void Board::doNullMove(MoveState& st) noexcept {
    //FIXME Eliminare costati magiche
    prepareNullMoveState(st);
    lastMoveChangeFlags = MOVE_CHANGE_NONE;
    uint64_t newHash = currentHash;
    if (st.prevEpHashFile < 8) {
        newHash ^= zobrist::TABLES.enPassant[st.prevEpHashFile];
    }

    enPassant = Coords{};
    epHashFile = 0xFF;

    if (halfMoveClock < 255) {
        ++halfMoveClock;
    }
    if (activeColor == BLACK && fullMoveClock < 255) {
        ++fullMoveClock;
    }
    activeColor = oppositeColor(activeColor);
    newHash ^= zobrist::TABLES.sideToMove;
    currentHash = newHash;
    // Null move is a search artifact and must not enter repetition history.
    // Otherwise threefold detection in subtree can be polluted.
}

__attribute__((hot))
void Board::undoNullMove(const MoveState& st) noexcept {
    restoreState(st);
}
} // namespace chess {
