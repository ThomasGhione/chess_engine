#include "board.hpp"
#include "../tt/zobrist.hpp"

namespace chess {

namespace {

// The castling rights a move from or to each square keeps (KQkq = bits 0..3):
// moving the king or a rook off its home square, or capturing on a rook's home
// square, gives the matching rights up.
constexpr std::array<uint8_t, 64> CASTLING_KEEP = [] {
    std::array<uint8_t, 64> keep{};
    keep.fill(0x0F);
    keep[0]  = 0x07; keep[4]  = 0x03; keep[7]  = 0x0B;   // a8 q, e8 kq, h8 k
    keep[56] = 0x0D; keep[60] = 0x0C; keep[63] = 0x0E;   // a1 Q, e1 KQ, h1 K
    return keep;
}();

} // namespace

__attribute__((always_inline))
inline void Board::set(uint8_t index, piece_id value) noexcept {
    chessboard[index] = static_cast<uint8_t>(value & MASK_PIECE);
}

// The queue cancels a delta against the pending one it inverts. undoMove emits
// exactly the inverse operations in reverse order, so a do/undo pair that never
// reached an evaluate annihilates here and never touches a row.
__attribute__((always_inline))
inline void Board::queueAcc(NNUE::AccDelta delta) const noexcept {
    const NNUE::AccDelta inverse = (delta.kind == NNUE::AccDelta::Move)
        ? NNUE::AccDelta{delta.kind, delta.piece, delta.to, delta.from}
        : NNUE::AccDelta{static_cast<uint8_t>(delta.kind ^ 1), delta.piece, delta.from, 0}; // Add <-> Remove
    if (accPendingCount > 0 && accPending[accPendingCount - 1] == inverse) {
        --accPendingCount;
        return;
    }
    if (accPendingCount == NNUE::MAX_ACC_PENDING) [[unlikely]] flushAccPending();
    accPending[accPendingCount++] = delta;
}

__attribute__((always_inline))
inline void Board::addPieceToBB(uint8_t piece, uint8_t index) noexcept {
    bb[piece & MASK_PIECE_TYPE][colorToIndex(piece)] |= 1ULL << index;
    if (NNUE::activeNetwork != nullptr) [[likely]] queueAcc({NNUE::AccDelta::Add, piece, index, 0});
}

__attribute__((always_inline))
inline void Board::removePieceFromBB(uint8_t piece, uint8_t index) noexcept {
    bb[piece & MASK_PIECE_TYPE][colorToIndex(piece)] &= ~(1ULL << index);
    if (NNUE::activeNetwork != nullptr) [[likely]] queueAcc({NNUE::AccDelta::Remove, piece, index, 0});
}

// Moves a piece between squares: mailbox, occupancy, its bitboard (which never
// holds the destination, so one XOR does both ends) and the accumulator, which
// gets one pass for the pair instead of a remove and an add.
__attribute__((always_inline))
inline void Board::relocate(uint8_t piece, uint8_t fromIndex, uint8_t toIndex) noexcept {
    set(toIndex, static_cast<piece_id>(piece));
    set(fromIndex, EMPTY);
    occupancy = (occupancy | (1ULL << toIndex)) & ~(1ULL << fromIndex);
    bb[piece & MASK_PIECE_TYPE][colorToIndex(piece)] ^= (1ULL << fromIndex) | (1ULL << toIndex);
    if (NNUE::activeNetwork != nullptr) [[likely]] queueAcc({NNUE::AccDelta::Move, piece, fromIndex, toIndex});
}

__attribute__((always_inline))
inline constexpr bool Board::isCaptureKind(MoveKind kind) noexcept {
    return kind == MoveKind::Capture || kind == MoveKind::PromotionCapture;
}

__attribute__((always_inline))
inline constexpr bool Board::isPromotionKind(MoveKind kind) noexcept {
    return kind == MoveKind::PromotionQuiet || kind == MoveKind::PromotionCapture;
}

inline Board::MoveKind Board::classifyMoveKind(uint8_t movingType, uint8_t movingColor, uint8_t fromIndex,
                                               uint8_t toIndex, uint8_t destBefore, const Square& prevEnPassant) noexcept {
    const MoveKind plain = (destBefore != EMPTY) ? MoveKind::Capture : MoveKind::Quiet;
    if (movingType == KING) {
        const int df = chess::file(toIndex) - chess::file(fromIndex);
        return (chess::rank(fromIndex) == chess::rank(toIndex) && (df == 2 || df == -2)) ? MoveKind::Castling : plain;
    }
    if (movingType != PAWN) return plain;
    if (chess::file(fromIndex) != chess::file(toIndex) && destBefore == EMPTY
        && isValidSquare(prevEnPassant) && toIndex == prevEnPassant) {
        return MoveKind::EnPassant;
    }
    const uint8_t toRank = chess::rank(toIndex);
    if (toRank == promotionRank(movingColor == WHITE)) {
        return (destBefore != EMPTY) ? MoveKind::PromotionCapture : MoveKind::PromotionQuiet;
    }
    const int dr = toRank - chess::rank(fromIndex);
    return (dr == 2 || dr == -2) ? MoveKind::DoublePawnPush : plain;
}

// Runs f.template operator()<Kind>() for the move kind known at runtime.
template<typename F>
__attribute__((always_inline))
inline void Board::visitKind(MoveKind kind, F&& f) noexcept {
    switch (kind) {
        case MoveKind::Capture:          f.template operator()<MoveKind::Capture>(); break;
        case MoveKind::DoublePawnPush:   f.template operator()<MoveKind::DoublePawnPush>(); break;
        case MoveKind::EnPassant:        f.template operator()<MoveKind::EnPassant>(); break;
        case MoveKind::Castling:         f.template operator()<MoveKind::Castling>(); break;
        case MoveKind::PromotionQuiet:   f.template operator()<MoveKind::PromotionQuiet>(); break;
        case MoveKind::PromotionCapture: f.template operator()<MoveKind::PromotionCapture>(); break;
        default:                         f.template operator()<MoveKind::Quiet>(); break;
    }
}

__attribute__((always_inline))
inline void Board::prepareMoveState(MoveState& st, uint8_t destBefore) const noexcept {
    st.prevHalfMoveClock = halfMoveClock;
    st.prevFullMoveClock = fullMoveClock;
    st.prevEnPassant     = enPassant;
    st.prevEpHashFile    = epHashFile;
    st.prevCastle        = castle;
    st.prevHistorySize   = historySize;
    st.prevPliesFromNull = pliesFromNull;
    st.prevHistoryHead   = currentHash;
    st.capturedPiece     = destBefore;
}

__attribute__((always_inline))
inline void Board::restoreState(const MoveState& st) noexcept {
    activeColor   = oppositeColor(activeColor);
    halfMoveClock = st.prevHalfMoveClock;
    fullMoveClock = st.prevFullMoveClock;
    enPassant     = st.prevEnPassant;
    epHashFile    = st.prevEpHashFile;
    castle        = st.prevCastle;
    // The slot doMove overwrote is historySize - 1 before shrinking back.
    repetitionHistory[historySize - 1] = st.prevHistorySlotValue;
    historySize   = st.prevHistorySize;
    pliesFromNull = st.prevPliesFromNull;
    currentHash   = st.prevHistoryHead;
}

__attribute__((always_inline))
inline void Board::passTurn(bool resetHalfMoves) noexcept {
    if (resetHalfMoves) halfMoveClock = 0;
    else if (halfMoveClock < 255) ++halfMoveClock;
    if (activeColor == BLACK && fullMoveClock < 255) ++fullMoveClock;
    activeColor = oppositeColor(activeColor);
}

template<Board::MoveKind Kind>
inline void Board::doMoveByKind(MoveState& st, uint8_t moving, uint8_t destBefore, uint8_t fromIndex,
                                uint8_t toIndex, uint8_t promotionType, uint64_t& key) noexcept {
    if constexpr (Kind == MoveKind::EnPassant) {
        const uint8_t capIndex = static_cast<uint8_t>(toIndex + ((moving & MASK_COLOR) == WHITE ? 8 : -8));
        st.capturedPiece = get(capIndex);
        st.enPassantCapturedIndex = capIndex;
        set(capIndex, EMPTY);
        occupancy &= ~(1ULL << capIndex);
        removePieceFromBB(st.capturedPiece, capIndex);
        key ^= zobrist::TABLES.pieces[st.capturedPiece][capIndex];
    }
    if constexpr (isCaptureKind(Kind)) {
        removePieceFromBB(destBefore, toIndex);
        key ^= zobrist::TABLES.pieces[destBefore][toIndex];
    }
    relocate(moving, fromIndex, toIndex);
    if constexpr (Kind == MoveKind::Castling) {
        const bool kingside = toIndex > fromIndex;
        st.rookFromIndex = (toIndex & ~7) | (kingside ? 7 : 0);
        st.rookToIndex   = (toIndex & ~7) | (kingside ? 5 : 3);
        const uint8_t rook = get(st.rookFromIndex);
        relocate(rook, st.rookFromIndex, st.rookToIndex);
        key ^= zobrist::TABLES.pieces[rook][st.rookFromIndex] ^ zobrist::TABLES.pieces[rook][st.rookToIndex];
    }
    castle &= CASTLING_KEEP[fromIndex] & CASTLING_KEEP[toIndex];
    if constexpr (Kind == MoveKind::DoublePawnPush) enPassant = (fromIndex + toIndex) >> 1;
    if constexpr (isPromotionKind(Kind)) {
        // A missing or invalid promotion piece means a queen.
        const uint8_t promo = (promotionType >= KNIGHT && promotionType <= QUEEN) ? promotionType : uint8_t{QUEEN};
        const uint8_t promoted = static_cast<uint8_t>(promo | (moving & MASK_COLOR));
        removePieceFromBB(moving, toIndex);
        addPieceToBB(promoted, toIndex);
        set(toIndex, static_cast<piece_id>(promoted));
        key ^= zobrist::TABLES.pieces[promoted][toIndex];
    } else {
        key ^= zobrist::TABLES.pieces[moving][toIndex];
    }
}

template<Board::MoveKind Kind>
inline void Board::undoMoveByKind(const MoveState& st, uint8_t pieceOnTo, uint8_t fromIndex, uint8_t toIndex) noexcept {
    if constexpr (isPromotionKind(Kind)) {
        const uint8_t pawn = PAWN | (pieceOnTo & MASK_COLOR);
        removePieceFromBB(pieceOnTo, toIndex);
        addPieceToBB(pawn, toIndex);
        set(toIndex, static_cast<piece_id>(pawn));
        pieceOnTo = pawn;
    }
    relocate(pieceOnTo, toIndex, fromIndex);
    if constexpr (Kind == MoveKind::EnPassant || isCaptureKind(Kind)) {
        const uint8_t sq = (Kind == MoveKind::EnPassant) ? st.enPassantCapturedIndex : toIndex;
        set(sq, static_cast<piece_id>(st.capturedPiece));
        occupancy |= 1ULL << sq;
        addPieceToBB(st.capturedPiece, sq);
    }
    if constexpr (Kind == MoveKind::Castling) relocate(get(st.rookToIndex), st.rookToIndex, st.rookFromIndex);
}

// ------------------------------------------------------------
// INCREMENTAL DO/UNDO MOVE (NO LEGALITY CHECKS)
// ------------------------------------------------------------
__attribute__((hot))
void Board::doMove(const Move& m, MoveState& st) noexcept {
    const uint8_t moving     = get(m.from);
    const uint8_t destBefore = get(m.to);
    prepareMoveState(st, destBefore);
    st.moveKind = classifyMoveKind(moving & MASK_PIECE_TYPE, moving & MASK_COLOR, m.from, m.to,
                                   destBefore, st.prevEnPassant);

    uint64_t key = currentHash ^ zobrist::TABLES.pieces[moving][m.from] ^ zobrist::TABLES.sideToMove;
    if (st.prevEpHashFile < 8) key ^= zobrist::TABLES.enPassant[st.prevEpHashFile];
    enPassant = NO_SQUARE;
    visitKind(st.moveKind, [&]<MoveKind K>() {
        doMoveByKind<K>(st, moving, destBefore, m.from, m.to, m.promotionType, key);
    });

    const bool resetHistory = (moving & MASK_PIECE_TYPE) == PAWN || destBefore != EMPTY;
    passTurn(resetHistory);
    if (castle != st.prevCastle) key ^= zobrist::TABLES.castling[st.prevCastle] ^ zobrist::TABLES.castling[castle];
    epHashFile = 0xFF;
    if (isValidSquare(enPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, enPassant)) {
        epHashFile = chess::file(enPassant);
        key ^= zobrist::TABLES.enPassant[epHashFile];
    }
    currentHash = key;
    if (pliesFromNull < 255) ++pliesFromNull;
    updateRepetitionAfterMove(resetHistory, st);
}

__attribute__((hot))
void Board::undoMove(const Move& m, const MoveState& st) noexcept {
    const uint8_t pieceOnTo = get(m.to);
    visitKind(st.moveKind, [&]<MoveKind K>() { undoMoveByKind<K>(st, pieceOnTo, m.from, m.to); });
    restoreState(st);
}

__attribute__((hot))
void Board::doNullMove(MoveState& st) noexcept {
    prepareMoveState(st, EMPTY);
    uint64_t key = currentHash ^ zobrist::TABLES.sideToMove;
    if (st.prevEpHashFile < 8) key ^= zobrist::TABLES.enPassant[st.prevEpHashFile];
    enPassant = NO_SQUARE;
    epHashFile = 0xFF;
    passTurn(false);
    currentHash = key;
    // The post-null position takes a history slot like any ply, so the same
    // side to move stays on every second slot; countRepetitions never looks
    // further back than this entry.
    updateRepetitionAfterMove(false, st);
    pliesFromNull = 0;
}

__attribute__((hot))
void Board::undoNullMove(const MoveState& st) noexcept {
    restoreState(st);
}

} // namespace chess
