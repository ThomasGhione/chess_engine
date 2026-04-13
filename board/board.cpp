#include "board.hpp"
#include "../tt/zobrist.hpp"

namespace chess {


bool Board::move(const Coords& from, const Coords& to, char promotionChoice) noexcept {    
    const uint8_t fromIndex = from.index;
    const uint8_t moving = get(fromIndex);
    const uint8_t movingColor = moving & MASK_COLOR;

    if (!isLegalPseudoMove(from.index, to.index, moving, inCheck(movingColor), false))
        return false;

    MoveState st{};
    doMove(Move{from, to, promotionChoice}, st, promotionChoice);
    return true;
}

bool Board::promote(const Coords& at, char choice) noexcept {
    const uint8_t piece = get(at);
    const uint8_t type = piece & MASK_PIECE_TYPE;
    if (type != PAWN) [[unlikely]] 
        return false; // must be a pawn
    
    const uint8_t color = piece & MASK_COLOR; // WHITE if set, otherwise BLACK
    if (rank(at.index) != promotionRank(color == WHITE)) [[unlikely]] 
        return false;

    const uint8_t promo = normalizePromotionChoice(choice);
    const uint8_t atIndex = at.index;
    promoteUnchecked(atIndex, piece, promo);
    return true;
}

bool Board::isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, bool inChk, bool inDoubleChk) const noexcept {
    return isLegalPseudoMove(fromIndex, toIndex, get(fromIndex), inChk, inDoubleChk);
}

bool Board::isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, uint8_t fromPiece, bool inChk, bool inDoubleChk) const noexcept {
    if (fromPiece == EMPTY) [[unlikely]] return false;

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
            const uint64_t bitMap = pieces::generateMovesByType<KNIGHT>(fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece);
        }
        case BISHOP: {
            const uint64_t bitMap = pieces::generateMovesByType<BISHOP>(fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece);
        }
        case ROOK: {
            const uint64_t bitMap = pieces::generateMovesByType<ROOK>(fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece);
        }
        case QUEEN: {
            const uint64_t bitMap = pieces::generateMovesByType<QUEEN>(fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
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
    
    uint8_t attackers = 0;

    const uint64_t pawnAtk = pieces::PAWN_ATTACKERS_TO[oppSide][kingIndex] & pawns_bb[oppSide];
    if (addAttackAndDetectDouble(pawnAtk, attackers)) return true;

    const uint64_t knightAtk = pieces::KNIGHT_ATTACKS[kingIndex] & knights_bb[oppSide];
    if (addAttackAndDetectDouble(knightAtk, attackers)) return true;

    const uint64_t kingAtk = pieces::KING_ATTACKS[kingIndex] & kings_bb[oppSide];
    if (addAttackAndDetectDouble(kingAtk, attackers)) return true;

    const uint64_t rookLike = rooks_bb[oppSide] | queens_bb[oppSide];
    if (rookLike) {
        const uint64_t rookAtk = pieces::getRookAttacks(kingIndex, occupancy) & rookLike;
        if (addAttackAndDetectDouble(rookAtk, attackers)) return true;
    }

    const uint64_t bishopLike = bishops_bb[oppSide] | queens_bb[oppSide];
    if (bishopLike) {
        const uint64_t bishopAtk = pieces::getBishopAttacks(kingIndex, occupancy) & bishopLike;
        if (addAttackAndDetectDouble(bishopAtk, attackers)) return true;
    }

    return false;
}

// Simple piece pseudo-legal check
[[nodiscard]] inline bool Board::isSimplePieceLegal(uint64_t bitMap, uint64_t toBit) noexcept {
    return (bitMap & toBit) != 0ULL;
}

// King move validation (normal moves + castling)
[[nodiscard]] inline bool Board::isKingMoveLegal(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint64_t toBit,
    uint8_t movingColor
) const noexcept {
    const uint8_t oppColor = oppositeColor(movingColor);
    const int fileDelta = file(toIndex) - file(fromIndex);
    const int rankDelta = rank(toIndex) - rank(fromIndex);

    // Handle castling explicitly when king moves two files on same rank
    if (rankDelta == 0 && (fileDelta == 2 || fileDelta == -2)) {
        return canCastleToSquare(fromIndex, movingColor, fileDelta == 2);
    }

    // Normal king move: one-step king attack and destination not attacked
    const uint64_t attacks = pieces::KING_ATTACKS[fromIndex];
    if ((attacks & toBit) == 0ULL) return false;
    if (isSquareAttacked(toIndex, oppColor, fromIndex)) return false;

    return true;
}

// Castling dispatcher — caller already guarantees rankDelta==0 and |fileDelta|==2
[[nodiscard]] inline bool Board::canCastleToSquare(
    uint8_t fromIndex,
    uint8_t movingColor,
    bool isKingside
) const noexcept {
    if (file(fromIndex) != 4) return false;
    
    const bool isWhite = (movingColor == WHITE);
    const uint8_t expectedRank = isWhite ? 7 : 0;

    if (rank(fromIndex) != expectedRank) return false;
    
    return canCastleGeneric(isWhite, fromIndex, isKingside);
}

// Generic castling validation (consolidated logic)
[[nodiscard]] inline bool Board::canCastleGeneric(
    bool isWhite,
    uint8_t fromIndex,
    bool isKingside
) const noexcept {
    const uint8_t side = colorToIndex(isWhite ? WHITE : BLACK);
    const uint8_t oppColor = oppositeColor(isWhite ? WHITE : BLACK);
    
    // Check castling rights
    const uint8_t rightBit = isWhite 
        ? (isKingside ? 0u : 1u)   // White O-O / O-O-O
        : (isKingside ? 2u : 3u);  // Black O-O / O-O-O
    
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
    return isCastlePathSafe(castlePath, oppColor);
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
    const uint64_t capturedEnemyMask = (destPiece != EMPTY)
        ? Board::bitMask(toIndex)
        : 0ULL;
    return isKingSafeAfterMove(movingColor, fromIndex, toIndex, capturedEnemyMask);
}

// ------------------------------------------------------------
// CHECK / CHECKMATE / STALEMATE UTILITIES
// ------------------------------------------------------------
bool Board::isSquareAttackedWithOcc(uint8_t targetIndex, uint8_t byColor, uint64_t occ) const noexcept {
    const uint8_t side = colorToIndex(byColor);
    return isKingAttackedCustom(targetIndex, side, occ,
                                pawns_bb[side], knights_bb[side], bishops_bb[side],
                                rooks_bb[side], queens_bb[side], kings_bb[side]);
}

// Returns true if square 'targetIndex' is attacked by 'byColor'
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept {
    return isSquareAttackedWithOcc(targetIndex, byColor, occupancy);
}


// Version that excludes a square from occupancy - useful for king moves
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept {
    const uint64_t occMinus = occupancy & ~Board::bitMask(excludeSquare);
    return isSquareAttackedWithOcc(targetIndex, byColor, occMinus);
}


// Returns true if all squares are safe, false if ANY square is attacked
// Used for castling to avoid 3 separate isSquareAttacked calls
bool Board::isCastlePathSafe(uint64_t squaresMask, uint8_t byColor) const noexcept {
    while (squaresMask) {
        const uint8_t sq = __builtin_ctzll(squaresMask);
        squaresMask &= squaresMask - 1;
        if (isSquareAttacked(sq, byColor)) return false;
    }
    return true;
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
            const uint64_t capturedMask = (toBit & enemyOcc) ? toBit : 0ULL;
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
    uint64_t kings = kings_bb[side];
    if (kings) [[likely]] {
        const uint8_t king = __builtin_ctzll(kings);
        uint64_t moves = pieces::KING_ATTACKS[king] & ~ownOcc;
        while (moves) {
            const uint8_t to = __builtin_ctzll(moves);
            moves &= moves - 1;
            if (isLegalPseudoMove(king, to, inChk, false)) return true;
        }
        
        if (!inChk) {
            const uint8_t eIndex = (side == 0) ? WHITE_KING_START : BLACK_KING_START;
            if (king == eIndex) {
                if (isLegalPseudoMove(eIndex, eIndex + 2, inChk, false)) return true;
                if (isLegalPseudoMove(eIndex, eIndex - 2, inChk, false)) return true;
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
        // Shift all entries one position to the left (discard oldest)
        for (uint8_t i = 1; i < repetitionHistory.size(); ++i) {
            repetitionHistory[i - 1] = repetitionHistory[i];
        }
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
