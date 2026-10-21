#include <algorithm>
#include "board.hpp"
#include "../tt/zobrist.hpp"

namespace chess {


bool Board::move(Move move) noexcept {
    const uint8_t moving = get(move.from);
    if (moving == EMPTY || (moving & MASK_COLOR) != activeColor) [[unlikely]]
        return false;
    if (!isLegalPseudoMove(move.from, move.to, moving)) [[unlikely]]
        return false;
    MoveState st{};
    doMove(move, st);
    return true;
}

bool Board::isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, uint8_t fromPiece) const noexcept {
    const uint8_t fromType = fromPiece & MASK_PIECE_TYPE;
    const uint8_t movingColor = fromPiece & MASK_COLOR;

    const uint8_t destPiece = get(toIndex);

    if (destPiece != EMPTY && (destPiece & MASK_COLOR) == movingColor) [[unlikely]]
        return false;

    const uint64_t toBit = Board::BIT_MASKS[toIndex];
    switch (fromType) {
        case PAWN: {
            const bool isWhite = (movingColor == WHITE);

            // Diagonal move (capture or en-passant)?
            if (pieces::PAWN_ATTACKS[colorToIndex(movingColor)][fromIndex] & toBit) {
                // En-passant: diagonal to empty square that matches EP target
                if (destPiece == EMPTY) {
                    if (isValidSquare(enPassant) && toIndex == enPassant) {
                        const int8_t epDir = isWhite ? 8 : -8;
                        const uint8_t capturedPawnIdx = toIndex + epDir;
                        return isKingSafeAfterMove(movingColor, fromIndex, toIndex, BIT_MASKS[capturedPawnIdx]);
                    }
                    return false; // diagonal to empty square but not en-passant
                }
                // Normal capture (destPiece is enemy)
                return isKingSafeAfterMove(movingColor, fromIndex, toIndex, toBit);
            }

            // Forward push: must land on valid push square and destination must be empty
            if (destPiece != EMPTY) [[unlikely]] return false;
            if (!(pieces::getPawnForwardPushes(fromIndex, isWhite, occupancy) & toBit)) return false;
            return isKingSafeAfterMove(movingColor, fromIndex, toIndex, 0ULL);
        }
        case KNIGHT: return pseudoMoveLegalByType<KNIGHT>(fromIndex, toIndex, toBit, movingColor, destPiece);
        case BISHOP: return pseudoMoveLegalByType<BISHOP>(fromIndex, toIndex, toBit, movingColor, destPiece);
        case ROOK:   return pseudoMoveLegalByType<ROOK>(fromIndex, toIndex, toBit, movingColor, destPiece);
        case QUEEN:  return pseudoMoveLegalByType<QUEEN>(fromIndex, toIndex, toBit, movingColor, destPiece);
        case KING: return isKingMoveLegal(fromIndex, toIndex, toBit, movingColor);
        default: return false;
    }
}

template<uint8_t PieceType>
inline bool Board::pseudoMoveLegalByType(uint8_t fromIndex, uint8_t toIndex, uint64_t toBit, uint8_t movingColor, uint8_t destPiece) const noexcept {
    if ((pieces::generateMovesByType<PieceType>(fromIndex, occupancy) & toBit) == 0ULL) [[unlikely]] return false;
    return isKingSafeAfterMove(movingColor, fromIndex, toIndex, (destPiece != EMPTY) ? toBit : 0ULL);
}

// ============================================
// HELPER FUNCTIONS FOR isLegalPseudoMove
// ============================================

[[nodiscard]] uint64_t Board::checkersTo(uint8_t color) const noexcept {
    const uint8_t side = colorToIndex(color);
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return 0ULL;
    const uint8_t kingIndex = std::countr_zero(kingBB);
    const uint8_t oppSide = side ^ 1;

    uint64_t checkers = (pieces::PAWN_ATTACKERS_TO[oppSide][kingIndex] & pawns_bb[oppSide])
                      | (pieces::KNIGHT_ATTACKS[kingIndex] & knights_bb[oppSide])
                      | (pieces::KING_ATTACKS[kingIndex] & kings_bb[oppSide]);

    const uint64_t rookLike = rooks_bb[oppSide] | queens_bb[oppSide];
    if (rookLike) {
        checkers |= (pieces::getRookAttacks(kingIndex, occupancy) & rookLike);
    }

    const uint64_t bishopLike = bishops_bb[oppSide] | queens_bb[oppSide];
    if (bishopLike) {
        checkers |= (pieces::getBishopAttacks(kingIndex, occupancy) & bishopLike);
    }

    return checkers;
}

[[nodiscard]] bool Board::isKingMoveLegal(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint64_t toBit,
    uint8_t movingColor
) const noexcept {
    const uint8_t oppColor = oppositeColor(movingColor);
    const int diff = static_cast<int>(toIndex) - fromIndex;

    // Castling moves are uniquely identified by a destination offset of +2 or -2.
    // Normal king moves have offsets of +/-1, +/-7, +/-8, +/-9, so they cannot clash.
    if (diff == 2 || diff == -2) [[unlikely]] {
        if (chess::file(fromIndex) != 4) return false;
        const bool isWhite = (movingColor == WHITE);
        const uint8_t expectedRank = isWhite ? 7 : 0;
        if (chess::rank(fromIndex) != expectedRank) return false;
        return canCastleGeneric(isWhite, fromIndex, diff == 2);
    }

    // Normal king move: one-step king attack and destination not attacked
    const uint64_t attacks = pieces::KING_ATTACKS[fromIndex];
    if ((attacks & toBit) == 0ULL) return false;
    if (isSquareAttacked(toIndex, oppColor, fromIndex)) return false;

    return true;
}

[[nodiscard]] bool Board::canCastleGeneric(
    bool isWhite,
    uint8_t fromIndex,
    bool isKingside
) const noexcept {
    const uint8_t side = isWhite ^ 1; // 0 for White, 1 for Black
    const uint8_t oppColor = isWhite ? BLACK : WHITE;
    
    // Check castling rights
    const uint8_t rightBit = (!isWhite << 1) | !isKingside;
    
    if ((castle & (1u << rightBit)) == 0u) return false;
    
    // Setup indices based on direction
    const int8_t direction = isKingside ? 1 : -1;
    const uint8_t sq1 = fromIndex + direction;
    const uint8_t sq2 = fromIndex + 2 * direction;
    const uint8_t rookIdx = isKingside ? (fromIndex + 3) : (fromIndex - 4);
    
    // Check empty squares (always check 2, for queenside check 3rd)
    if (get(sq1) != EMPTY || get(sq2) != EMPTY)
        return false;
    
    if (!isKingside && get(fromIndex - 3) != EMPTY)
        return false;
    
    // Check rook presence
    if ((rooks_bb[side] & Board::BIT_MASKS[rookIdx]) == 0ULL)
        return false;
    
    if (isSquareAttacked(fromIndex, oppColor)) return false;
    if (isSquareAttacked(sq1, oppColor)) return false;
    if (isSquareAttacked(sq2, oppColor)) return false;

    return true;
}

// ------------------------------------------------------------
// CHECK / CHECKMATE / STALEMATE UTILITIES
// ------------------------------------------------------------
// Returns true if square 'targetIndex' is attacked by 'byColor'
// Version that excludes a square from occupancy - useful for king moves
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept {
    const uint64_t occMinus = excludeSquare < 64 ? (occupancy & ~Board::BIT_MASKS[excludeSquare]) : occupancy;
    const uint8_t side = colorToIndex(byColor);
    return isKingAttackedCustom(targetIndex, side, occMinus,
                                pawns_bb[side], knights_bb[side], bishops_bb[side],
                                rooks_bb[side], queens_bb[side], kings_bb[side]);
}

// Helper: check if king at kingSq is attacked using custom bitboards
// Used internally to avoid code duplication when simulating moves
bool Board::isKingAttackedCustom(uint8_t kingSq, uint8_t bySide, uint64_t occ,
                                 uint64_t pawns, uint64_t knights, uint64_t bishops,
                                 uint64_t rooks, uint64_t queens, uint64_t kings) noexcept {
    if (pieces::PAWN_ATTACKERS_TO[bySide][kingSq] & pawns) return true;
    if (pieces::KNIGHT_ATTACKS[kingSq] & knights) return true;
    if (pieces::KING_ATTACKS[kingSq] & kings) return true;
    
    const uint64_t rookLike = rooks | queens;
    if (rookLike && (pieces::getRookAttacks(kingSq, occ) & rookLike)) return true;

    const uint64_t bishopLike = bishops | queens;
    if (bishopLike && (pieces::getBishopAttacks(kingSq, occ) & bishopLike)) return true;
    
    return false;
}

__attribute__((hot))
bool Board::inCheck(uint8_t color) const noexcept {
    const uint8_t side = colorToIndex(color);
    const uint64_t kingBB = kings_bb[side];

    if (!kingBB) [[unlikely]] return false;
    const uint8_t kingSq = std::countr_zero(kingBB);
    const uint8_t bySide = side ^ 1;
    return isKingAttackedCustom(kingSq, bySide, occupancy,
                                pawns_bb[bySide], knights_bb[bySide], bishops_bb[bySide],
                                rooks_bb[bySide], queens_bb[bySide], kings_bb[bySide]);
}


void Board::recomputeHashAndEp() noexcept {
    currentHash = zobrist::computeHashKey(*this);
    epHashFile = 0xFF;
    if (zobrist::hasPseudoLegalEnPassantCapture(*this, getEnPassant()))
        epHashFile = chess::file(getEnPassant());
}

void Board::rebuildRepetitionHistory() noexcept {
    recomputeHashAndEp();
    historySize = 0;
    pliesFromNull = 255;
    repetitionHistory[historySize++] = currentHash;
}

void Board::updateRepetitionAfterMove(bool resetHistory, MoveState& st) noexcept {
    if (resetHistory)
        historySize = 0;

    if (historySize >= repetitionHistory.size()) {
        std::memmove(repetitionHistory.data(), repetitionHistory.data() + 1, (repetitionHistory.size() - 1) * sizeof(uint64_t));
        historySize = static_cast<uint8_t>(repetitionHistory.size() - 1);
    }
    // Remember the slot we are about to overwrite so undoMove can restore it
    // (writeIndex == historySize here; on undo historySize-1 recovers it). This
    // is what keeps a reset-to-0 on an irreversible move from corrupting earlier
    // entries in the parent line.
    st.prevHistorySlotValue = repetitionHistory[historySize];
    repetitionHistory[historySize++] = currentHash;
}

// Occurrences of the current position, itself included. Only the same side to
// move can match, and every ply (null moves too) takes one slot, so the scan
// steps by two. It stops at the last null move: no game can repeat a position
// across one, the same bound Stockfish applies (pliesFromNull).
int Board::countRepetitions() const noexcept {
    const int last = static_cast<int>(historySize) - 1;
    const int stop = std::max(0, last - static_cast<int>(pliesFromNull));
    int count = 0;
    for (int i = last; i >= stop; i -= 2) {
        count += (repetitionHistory[static_cast<size_t>(i)] == currentHash);
    }
    return count;
}

bool Board::hasInsufficientMaterialDraw() const noexcept {
    if (pawns_bb[0] || pawns_bb[1] || rooks_bb[0] || rooks_bb[1] || queens_bb[0] || queens_bb[1]) return false;
    const uint64_t wMinors = knights_bb[0] | bishops_bb[0];
    const uint64_t bMinors = knights_bb[1] | bishops_bb[1];
    return (std::popcount(wMinors) <= 1 && bMinors == 0ULL)
        || (std::popcount(bMinors) <= 1 && wMinors == 0ULL);
}

} // namespace chess
