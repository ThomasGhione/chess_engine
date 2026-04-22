#include "board.hpp"
#include "../tt/zobrist.hpp"

namespace chess {


bool Board::move(const Coords& from, const Coords& to, char promotionChoice) noexcept {    
    const uint8_t moving = get(from.index);

    if (!isLegalPseudoMove(from.index, to.index, moving, inCheck(moving & MASK_COLOR), false)) [[unlikely]]
        return false;

    MoveState st{};
    doMove(Move{from, to, promotionChoice}, st, promotionChoice);
    return true;
}

bool Board::promote(const Coords& at, char choice) noexcept {
    const uint8_t piece = get(at);
    if ((piece & MASK_PIECE_TYPE) != PAWN) [[unlikely]] 
        return false; // must be a pawn
    if (rank(at.index) != promotionRank((piece & MASK_COLOR) == WHITE)) [[unlikely]] 
        return false;

    promoteUnchecked(at.index, piece, normalizePromotionChoice(choice));
    return true;
}

bool Board::isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, uint8_t fromPiece, bool inChk, bool inDoubleChk) const noexcept {
    const uint8_t fromType = fromPiece & MASK_PIECE_TYPE;
    const uint8_t movingColor = fromPiece & MASK_COLOR;

    const uint8_t destPiece = get(toIndex);

    if (destPiece != EMPTY && (destPiece & MASK_COLOR) == movingColor) [[unlikely]]
        return false;

    if (inChk && inDoubleChk && fromType != KING) [[unlikely]]
        return false;

    const uint64_t toBit = Board::bitMask(toIndex);
    switch (fromType) {
        case PAWN: {
            const bool isWhite = (movingColor == WHITE);
            const int side = colorToIndex(movingColor);

            // Diagonal move (capture or en-passant)?
            if (pieces::PAWN_ATTACKS[side][fromIndex] & toBit) {
                // En-passant: diagonal to empty square that matches EP target
                if (destPiece == EMPTY) {
                    if (Coords::isInBounds(enPassant) && toIndex == enPassant.index) {
                        const int8_t epDir = isWhite ? 8 : -8;
                        const uint8_t capturedPawnIdx = toIndex + epDir;
                        return isKingSafeAfterEnPassant(movingColor, fromIndex, toIndex, capturedPawnIdx);
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
        case KNIGHT: {
            if ((pieces::generateMovesByType<KNIGHT>(fromIndex, occupancy) & toBit) == 0ULL) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece);
        }
        case BISHOP: {
            if ((pieces::generateMovesByType<BISHOP>(fromIndex, occupancy) & toBit) == 0ULL) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece);
        }
        case ROOK: {
            if ((pieces::generateMovesByType<ROOK>(fromIndex, occupancy) & toBit) == 0ULL) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece);
        }
        case QUEEN: {
            if ((pieces::generateMovesByType<QUEEN>(fromIndex, occupancy) & toBit) == 0ULL) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece);
        }
        case KING:
            return isKingMoveLegal(fromIndex, toIndex, toBit, movingColor);
        default:
            return false;
    }
}

// ============================================
// HELPER FUNCTIONS FOR isLegalPseudoMove
// ============================================

// Lazy double-check detection - called ONLY when inChk=true && fromType != KING
[[nodiscard]] inline bool Board::isDoubleCheck(uint8_t movingColor) const noexcept {
    const uint8_t side = colorToIndex(movingColor);
    if (!kings_bb[side]) [[unlikely]] return false; // malformed position guard
    const uint8_t kingIndex = __builtin_ctzll(kings_bb[side]);
    const uint8_t oppSide = side ^ 1;
    
    // Accumulate all attackers in a single bitboard
    uint64_t attackers = (pieces::PAWN_ATTACKERS_TO[oppSide][kingIndex] & pawns_bb[oppSide])
                       | (pieces::KNIGHT_ATTACKS[kingIndex] & knights_bb[oppSide]);

    const uint64_t rookLike = rooks_bb[oppSide] | queens_bb[oppSide];
    if (rookLike) {
        attackers |= (pieces::getRookAttacks(kingIndex, occupancy) & rookLike);
    }

    const uint64_t bishopLike = bishops_bb[oppSide] | queens_bb[oppSide];
    if (bishopLike) {
        attackers |= (pieces::getBishopAttacks(kingIndex, occupancy) & bishopLike);
    }

    // A double check means at least 2 distinct pieces are attacking the king.
    // If the bitboard has more than 1 bit set, clearing the LSB will leave a non-zero value.
    return (attackers & (attackers - 1)) != 0ULL;
}

// King move validation (normal moves + castling)
[[nodiscard]] inline bool Board::isKingMoveLegal(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint64_t toBit,
    uint8_t movingColor
) const noexcept {
    const uint8_t oppColor = oppositeColor(movingColor);
    const int diff = (int)toIndex - (int)fromIndex;

    // Castling moves are uniquely identified by a destination offset of +2 or -2.
    // Normal king moves have offsets of +/-1, +/-7, +/-8, +/-9, so they cannot clash.
    if (diff == 2 || diff == -2) [[unlikely]] {
        if (file(fromIndex) != 4) return false;
        const bool isWhite = (movingColor == WHITE);
        const uint8_t expectedRank = isWhite ? 7 : 0;
        if (rank(fromIndex) != expectedRank) return false;
        return canCastleGeneric(isWhite, fromIndex, diff == 2);
    }

    // Normal king move: one-step king attack and destination not attacked
    const uint64_t attacks = pieces::KING_ATTACKS[fromIndex];
    if ((attacks & toBit) == 0ULL) return false;
    if (isSquareAttacked(toIndex, oppColor, fromIndex)) return false;

    return true;
}

// Generic castling validation (consolidated logic)
[[nodiscard]] inline bool Board::canCastleGeneric(
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
    if ((rooks_bb[side] & Board::bitMask(rookIdx)) == 0ULL)
        return false;
    
    // Check castle path safety
    const uint64_t castlePath = Board::bitMask(fromIndex) | Board::bitMask(sq1) | Board::bitMask(sq2);
    uint64_t mask = castlePath;
    while (mask) {
        const uint8_t sq = __builtin_ctzll(mask);
        mask &= mask - 1;
        if (isSquareAttacked(sq, oppColor)) return false;
    }
    return true;
}

// King safety check for non-king, non-pawn pieces
[[nodiscard]] inline bool Board::verifyKingSafetyForSimplePiece(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t movingColor,
    uint8_t destPiece
) const noexcept {
    // Note: own-color captures are already rejected by isLegalPseudoMove,
    // so if destPiece != EMPTY it is guaranteed to be an enemy piece.
    const uint64_t captureMask = static_cast<uint64_t>(-static_cast<int64_t>(destPiece != EMPTY));
    const uint64_t capturedEnemyMask = Board::bitMask(toIndex) & captureMask;
    return isKingSafeAfterMove(movingColor, fromIndex, toIndex, capturedEnemyMask);
}

// ------------------------------------------------------------
// CHECK / CHECKMATE / STALEMATE UTILITIES
// ------------------------------------------------------------
// Returns true if square 'targetIndex' is attacked by 'byColor'
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept {
    const uint8_t side = colorToIndex(byColor);
    return isKingAttackedCustom(targetIndex, side, occupancy,
                                pawns_bb[side], knights_bb[side], bishops_bb[side],
                                rooks_bb[side], queens_bb[side], kings_bb[side]);
}


// Version that excludes a square from occupancy - useful for king moves
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept {
    const uint64_t occMinus = occupancy & ~Board::bitMask(excludeSquare);
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
    const uint64_t bishopLike = bishops | queens;
    if ((rookLike | bishopLike) == 0ULL) return false;

    if (rookLike && (pieces::getRookAttacks(kingSq, occ) & rookLike)) return true;
    if (bishopLike && (pieces::getBishopAttacks(kingSq, occ) & bishopLike)) return true;
    
    return false;
}

__attribute__((hot))
bool Board::inCheck(uint8_t color) const noexcept {
    const uint8_t side = colorToIndex(color);
    const uint64_t kingBB = kings_bb[side];

    if (!kingBB) [[unlikely]] return false;
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    const uint8_t bySide = side ^ 1;
    return isKingAttackedCustom(kingSq, bySide, occupancy,
                                pawns_bb[bySide], knights_bb[bySide], bishops_bb[bySide],
                                rooks_bb[bySide], queens_bb[bySide], kings_bb[bySide]);
}


template<uint8_t PieceType>
[[nodiscard]] static inline bool hasLegalMovesForPieceType(
    const Board* board,
    uint64_t pieceBB,
    uint64_t ownOcc,
    uint64_t enemyOcc,
    uint64_t occupancy,
    uint8_t movingColor
) noexcept {
    while (pieceBB) {
        const uint8_t from = __builtin_ctzll(pieceBB);
        pieceBB &= pieceBB - 1;
        
        uint64_t movesMask = pieces::generateMovesByType<PieceType>(from, occupancy) & ~ownOcc;
        while (movesMask) {
            const uint8_t to = __builtin_ctzll(movesMask);
            movesMask &= movesMask - 1;
            const uint64_t toBit = Board::bitMask(to);
            const uint64_t capturedMask = toBit & enemyOcc;
            if (board->isKingSafeAfterMove(movingColor, from, to, capturedMask)) return true;
        }
    }
    return false;
}


bool Board::hasAnyLegalMove(uint8_t color) const noexcept {
    const int side = colorToIndex(color);
    const int oppSide = side ^ 1;

    const bool inChk = inCheck(color);
    const bool inDoubleChk = inChk && isDoubleCheck(color);

    const uint64_t ownOcc = pawns_bb[side] | knights_bb[side] | bishops_bb[side] |
                             rooks_bb[side] | queens_bb[side]  | kings_bb[side];
    const uint64_t enemyOcc = pawns_bb[oppSide] | knights_bb[oppSide] | bishops_bb[oppSide] |
                               rooks_bb[oppSide] | queens_bb[oppSide]  | kings_bb[oppSide];

    // --- KING MOVES (always exists, cheap to test) ---
    const uint64_t kings = kings_bb[side];
    if (kings) [[likely]] {
        const uint8_t king = __builtin_ctzll(kings);
        uint64_t moves = pieces::KING_ATTACKS[king] & ~ownOcc;
        while (moves) {
            const uint8_t to = __builtin_ctzll(moves);
            moves &= moves - 1;
            if (!isSquareAttacked(to, oppSide << 3, king)) return true;
        }
        
        if (!inChk) {
            const uint8_t eIndex = (side == 0) ? 60 : 4;  // WHITE_KING_START = 60, BLACK_KING_START = 4
            if (king == eIndex) {
                if (canCastleGeneric(side == 0, eIndex, true)) return true;
                if (canCastleGeneric(side == 0, eIndex, false)) return true;
            }
        }
    }

    if (inDoubleChk) return false;

    // --- NON-KING PIECES: skip isLegalPseudoMove, call isKingSafeAfterMove directly ---

    if (hasLegalMovesForPieceType<KNIGHT>(this, knights_bb[side], ownOcc, enemyOcc, occupancy, color))
        return true;

    const bool isWhite = (side == 0);
    uint64_t pawns = pawns_bb[side];
    while (pawns) {
        const uint8_t from = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        uint64_t push = pieces::getPawnForwardPushes(from, isWhite, occupancy);
        while (push) {
            const uint8_t to = __builtin_ctzll(push);
            push &= push - 1;
            if (isKingSafeAfterMove(color, from, to, 0ULL)) return true;
        }

        uint64_t caps = pieces::PAWN_ATTACKS[side][from] & enemyOcc;
        while (caps) {
            const uint8_t to = __builtin_ctzll(caps);
            caps &= caps - 1;
            if (isKingSafeAfterMove(color, from, to, bitMask(to))) return true;
        }
    }

    if (hasLegalMovesForPieceType<BISHOP>(this, bishops_bb[side], ownOcc, enemyOcc, occupancy, color))
        return true;

    if (hasLegalMovesForPieceType<ROOK>(this, rooks_bb[side], ownOcc, enemyOcc, occupancy, color))
        return true;

    if (hasLegalMovesForPieceType<QUEEN>(this, queens_bb[side], ownOcc, enemyOcc, occupancy, color))
        return true;

    return false;
}

void Board::rebuildRepetitionHistory() noexcept {
    currentHash = zobrist::computeHashKey(*this);
    epHashFile = 0xFF;
    if (Coords::isInBounds(enPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, enPassant)) {
        epHashFile = enPassant.file();
    }
    historySize = 0;
    repetitionHistory[historySize++] = currentHash;
}

void Board::updateRepetitionAfterMove(bool resetHistory, bool recomputeHash) noexcept {
    if (recomputeHash) {
        currentHash = zobrist::computeHashKey(*this);
        epHashFile = 0xFF;
        if (Coords::isInBounds(enPassant) && zobrist::hasPseudoLegalEnPassantCapture(*this, enPassant)) {
            epHashFile = enPassant.file();
        }
    }

    if (resetHistory) 
        historySize = 0;
    
    if (historySize >= repetitionHistory.size()) {
        // Shift all entries one position to the left (discard oldest), using memmove for vectorization
        std::memmove(repetitionHistory.data(), repetitionHistory.data() + 1, (repetitionHistory.size() - 1) * sizeof(uint64_t));
        historySize = repetitionHistory.size() - 1;
    }
    repetitionHistory[historySize++] = currentHash;
}

bool Board::isThreefoldRepetition() const noexcept {
    if (historySize == 0) return false;
    const uint64_t target = currentHash;
    int count = 1; // FIX: Start at 1 because currentHash is already in history (last entry)
    // Search history excluding the last entry (which is currentHash)
    for (uint8_t i = historySize - 1; i > 0; --i) {
        if (repetitionHistory[i - 1] == target) {
            if (++count >= 3) return true;
        }
    }
    return false;
}

bool Board::isTwofoldRepetition() const noexcept {
    if (historySize == 0) return false;
    const uint64_t target = currentHash;
    for (uint8_t i = historySize - 1; i > 0; --i) {
        if (repetitionHistory[i - 1] == target) {
            return true; // Position seen at least once before
        }
    }
    return false;
}

}; // namespace chess
