#include "board.hpp"
#include "../tt/zobrist.hpp"
#ifdef DEBUG
#include <iostream>
#endif

namespace chess {


bool Board::moveBB(const Coords& from, const Coords& to) noexcept {   
    const uint8_t moving = get(from);
    const uint8_t movingColor = moving & MASK_COLOR;
    
    if (!canMoveToBB(from, to, inCheck(movingColor)))
        return false;

    const uint8_t fromIndex = from.index;
    const uint8_t toIndex = to.index;
    const uint8_t movingType = moving & MASK_PIECE_TYPE;
    const uint8_t destBefore = get(to);

    // Clear en passant by default; may set again after a double push
    Coords prevEp = enPassant;
    enPassant = Coords{};

    // Track if this is an en passant capture for halfMoveClock reset
    bool wasEnPassantCapture = false;

    // Handle en passant capture: pawn moves diagonally into empty ep square
    if (movingType == PAWN) {
        // Check if pawn moved diagonally (different file)
        if (fileOf(fromIndex) != fileOf(toIndex) && destBefore == EMPTY && Coords::isInBounds(prevEp) && (toIndex == prevEp.index)) {
            wasEnPassantCapture = true;
            // Coords convention: index increases with rank (a8=0 -> h1=63)
            // White moves toward rank 0 (decreasing), Black toward rank 7 (increasing)
            // Captured pawn is one rank "behind" where the capturing pawn lands
            // For White capturing: captured pawn is at higher rank (toIndex + 8)
            // For Black capturing: captured pawn is at lower rank (toIndex - 8)
            const int8_t captureOffset = (movingColor == WHITE) ? 8 : -8;
            const uint8_t capturedIndex = toIndex + captureOffset;
            const uint8_t capturedPawn = get(capturedIndex);
            
            set(capturedIndex, EMPTY);
            occupancy &= ~Board::bitMask(capturedIndex);
            removePieceFromBB(capturedPawn, capturedIndex);
        }
    }

    // Move the piece
    updateChessboard(from, to, static_cast<piece_id>(moving));
    fastUpdateOccupancyBB(fromIndex, toIndex);

    // CRITICAL FIX: NON chiamare updateOccupancyBB() completo!
    // fastUpdateOccupancyBB + removePieceFromBB/addPieceToBB sono sufficienti
    // updateOccupancyBB() scansiona tutte le 64 caselle → ~200-300 cicli CPU sprecati!
    // RIMOSSO: updateOccupancyBB();
    
    // Update per-piece bitboards incrementally
    if (destBefore != EMPTY) {
        removePieceFromBB(destBefore, toIndex);
    }
    removePieceFromBB(moving, fromIndex);
    addPieceToBB(moving, toIndex);

    // Castling rook move if king moved two squares on same rank
    if (movingType == KING && rankOf(fromIndex) == rankOf(toIndex)) {
        const int df = static_cast<int>(fileOf(toIndex)) - static_cast<int>(fileOf(fromIndex));
        if (df == 2) {
            // kingside: rook h -> f
            const uint8_t rookFromIndex = toIndex + 1; // h = f + 2, simplified to toIndex + 1
            const uint8_t rookToIndex = toIndex - 1;   // f = g - 1, simplified to toIndex - 1
            const uint8_t rook = get(rookFromIndex);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                set(rookToIndex, static_cast<piece_id>(rook));
                set(rookFromIndex, EMPTY);
                fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
                removePieceFromBB(rook, rookFromIndex);
                addPieceToBB(rook, rookToIndex);
            }
        } else if (df == -2) {
            // queenside: rook a -> d
            const uint8_t rookFromIndex = toIndex - 2; // a = c - 2, simplified to toIndex - 2
            const uint8_t rookToIndex = toIndex + 1;   // d = c + 1, simplified to toIndex + 1
            const uint8_t rook = get(rookFromIndex);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                set(rookToIndex, static_cast<piece_id>(rook));
                set(rookFromIndex, EMPTY);
                fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
                removePieceFromBB(rook, rookFromIndex);
                addPieceToBB(rook, rookToIndex);
            }
        }
    }

    // Update castling rights for king/rook moves or rook captures
    // OTTIMIZZAZIONE: usa bitwise ops dirette invece di lambda
    if (movingType == KING) {
        const uint8_t castleMask = (movingColor == WHITE) ? 0x03 : 0x0C;  // bits 0-1 or bits 2-3
        const uint8_t kingBit = (movingColor == WHITE) ? 0x01 : 0x08;  // bit 0 or bit 3
        castle &= ~castleMask;
        hasMoved |= kingBit;
    }
    
    if (movingType == ROOK) {
        // White rooks at rank 7 (row 1), Black rooks at rank 0 (row 8)
        const bool isInitialSquare = (movingColor == WHITE)
            ? (fromIndex == WHITE_ROOK_A_START || fromIndex == WHITE_ROOK_H_START)
            : (fromIndex == BLACK_ROOK_A_START || fromIndex == BLACK_ROOK_H_START);
        
        if (isInitialSquare) {
            if (movingColor == WHITE) {
                if (fromIndex == WHITE_ROOK_A_START) {
                    castle &= ~(1u << WHITE_QUEENSIDE);
                    hasMoved |= (1u << 1);
                } else {
                    castle &= ~(1u << WHITE_KINGSIDE);
                    hasMoved |= (1u << 2);
                }
            } else {
                if (fromIndex == BLACK_ROOK_A_START) {
                    castle &= ~(1u << BLACK_QUEENSIDE);
                    hasMoved |= (1u << 4);
                } else {
                    castle &= ~(1u << BLACK_KINGSIDE);
                    hasMoved |= (1u << 5);
                }
            }
        }
    }
    
    // If a rook was captured on its starting square, disable that side's castling
    if (destBefore != EMPTY && ((destBefore & MASK_PIECE_TYPE) == ROOK)) {
        const bool isInitialSquare = ((destBefore & MASK_COLOR) == WHITE)
            ? (toIndex == WHITE_ROOK_A_START || toIndex == WHITE_ROOK_H_START)
            : (toIndex == BLACK_ROOK_A_START || toIndex == BLACK_ROOK_H_START);
        
        if (isInitialSquare) {
            if ((destBefore & MASK_COLOR) == WHITE) {
                castle &= (toIndex == WHITE_ROOK_A_START) 
                    ? ~(1u << WHITE_QUEENSIDE)
                    : ~(1u << WHITE_KINGSIDE);
            } else {
                castle &= (toIndex == BLACK_ROOK_A_START)
                    ? ~(1u << BLACK_QUEENSIDE)
                    : ~(1u << BLACK_KINGSIDE);
            }
        }
    }

    // Set en passant target if the move was a double pawn push
    if (movingType == PAWN) {
        // Optimize: use rankOf() dispatcher
        const int dr = static_cast<int>(rankOf(toIndex)) - static_cast<int>(rankOf(fromIndex));
        if (dr == 2 || dr == -2) {
            const uint8_t midIndex = (fromIndex + toIndex) >> 1; // Average of indices
            enPassant = Coords{midIndex};
        }
    }

    // --- UPDATE HALFMOVE CLOCK (50-move rule) ---
    // Reset to 0 if: pawn move, capture, or en passant capture
    // Otherwise increment
    if (movingType == PAWN || destBefore != EMPTY || wasEnPassantCapture) {
        halfMoveClock = 0;
    } else if (halfMoveClock < 255) {
        ++halfMoveClock;
    }

    // --- UPDATE SIDE TO MOVE AND FULLMOVE CLOCK ---
    if (activeColor == WHITE) {
        activeColor = BLACK;
    } else {
        activeColor = WHITE;
        if (fullMoveClock < 255) {
            ++fullMoveClock;
        }
    }

    const bool resetHistory = (movingType == PAWN) || (destBefore != EMPTY) || wasEnPassantCapture;
    updateRepetitionAfterMove(resetHistory);

    return true;
}

// Promote a pawn at 'at' using the provided choice char: 'q','r','b','n' (case-insensitive).
// Returns false if the piece is not a pawn on its promotion rank, otherwise promotes and returns true.
bool Board::promote(const Coords& at, char choice) noexcept {
    const uint8_t piece = get(at);
    const uint8_t type = piece & MASK_PIECE_TYPE;
    if (type != PAWN) [[unlikely]] return false; // must be a pawn
    const uint8_t color = piece & MASK_COLOR; // BLACK if set, otherwise WHITE
    // Verify pawn is on promotion rank according to color
    const uint8_t rank = rankOf(at.index);
    if (rank != promotionRank(color == WHITE)) [[unlikely]] return false;

    choice = static_cast<char>(std::tolower(static_cast<unsigned char>(choice)));
    uint8_t newType = QUEEN; // default promotion
    switch (choice) {
        case 'q': newType = QUEEN;  break;
        case 'r': newType = ROOK;   break;
        case 'b': newType = BISHOP; break;
        case 'n': newType = KNIGHT; break;
        // default to queen
    }

    const uint8_t newPiece = newType | color;
    
    // Update bitboards: remove pawn, add promoted piece
    const uint8_t atIndex = at.index;
    removePieceFromBB(piece, atIndex);
    addPieceToBB(newPiece, atIndex);
    
    // Update chessboard array
    set(at, static_cast<piece_id>(newPiece));
    // Occupancy remains unchanged (piece stays on the same square)
    return true;
}

// Overload: execute move and, if a pawn reaches last rank, promote using provided choice
bool Board::moveBB(const Coords& from, const Coords& to, char promotionChoice) noexcept {    
    if (!moveBB(from, to)) {
        return false;
    }
    return promote(to, promotionChoice);
}

bool Board::canMoveToBB(const Coords& from, const Coords& to, bool inChk) const noexcept {
    // ============================================
    // PHASE 1: FAST PATH - Early Validation
    // ============================================
    const uint8_t fromIndex = from.index;
    const uint8_t toIndex = to.index;
    const uint64_t toBit = Board::bitMask(toIndex);
    
    const uint8_t fromPiece = get(from);
    const uint8_t fromType = fromPiece & MASK_PIECE_TYPE;
    const uint8_t movingColor = fromPiece & MASK_COLOR;
    
    const uint8_t destPiece = get(to);
    const uint8_t destColor = destPiece & MASK_COLOR;
    
    // Early exit: can't capture own piece
    if (destPiece != EMPTY && destColor == movingColor) [[unlikely]] {
        return false;
    }
    
    // Lazy double-check evaluation
    if (inChk && fromType != KING) [[unlikely]] {
        if (isDoubleCheck(movingColor)) [[unlikely]] {
            return false; // Only king moves allowed in double-check
        }
    }
    
    // ============================================
    // PHASE 2: PSEUDO-LEGAL GENERATION + VALIDATION
    // ============================================
    switch (fromType) {
        case PAWN:
            return isPawnMoveLegal(fromIndex, toIndex, toBit, movingColor, destPiece, destColor);
        case KNIGHT:
        case BISHOP:
        case ROOK:
        case QUEEN: {
            // Use dispatch table for simple pieces (eliminates branch misprediction)
            const uint64_t bitMap = pieces::dispatchPieceMoves(fromType, fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece, destColor);
        }
        case KING:
            return isKingMoveLegal(fromIndex, toIndex, toBit, movingColor);
        
    }
    return false;
}

// ============================================
// HELPER FUNCTIONS FOR canMoveToBB
// ============================================

// Lazy double-check detection - called ONLY when inChk=true && fromType != KING
[[nodiscard]] inline bool Board::isDoubleCheck(uint8_t movingColor) const noexcept {
    const uint8_t side = colorToIndex(movingColor);
    if (!kings_bb[side]) [[unlikely]] return false; // malformed position guard
    const uint8_t kingIndex = __builtin_ctzll(kings_bb[side]);
    const uint8_t oppSide = side ^ 1;
    
    uint_fast32_t attackers = 0;
    
    // Fast path: non-sliding pieces
    attackers += __builtin_popcountll(pieces::PAWN_ATTACKERS_TO[oppSide][kingIndex] & pawns_bb[oppSide]);
    if (attackers >= 2) return true;
    
    attackers += __builtin_popcountll(pieces::KNIGHT_ATTACKS[kingIndex] & knights_bb[oppSide]);
    if (attackers >= 2) return true;
    
    attackers += __builtin_popcountll(pieces::KING_ATTACKS[kingIndex] & kings_bb[oppSide]);
    if (attackers >= 2) return true;
    
    // Sliding pieces
    attackers += __builtin_popcountll(pieces::getRookAttacks(kingIndex, occupancy) & 
                                      (rooks_bb[oppSide] | queens_bb[oppSide]));
    if (attackers >= 2) return true;
    
    attackers += __builtin_popcountll(pieces::getBishopAttacks(kingIndex, occupancy) & 
                                      (bishops_bb[oppSide] | queens_bb[oppSide]));
    
    return attackers >= 2;
}

// Pawn move validation with optimized en-passant
[[nodiscard]] inline bool Board::isPawnMoveLegal(
    uint8_t fromIndex, 
    uint8_t toIndex,
    uint64_t toBit,
    uint8_t movingColor,
    uint8_t destPiece,
    uint8_t destColor
) const noexcept {
    const bool isWhite = (movingColor == WHITE);
    const uint8_t side = colorToIndex(movingColor);
    const uint8_t oppSide = side ^ 1;
    const uint8_t oppColor = oppositeColor(movingColor);
    
    // Generate attacks and pushes using magic bitboards
    const uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][fromIndex];
    const uint64_t pushes  = pieces::getPawnForwardPushes(fromIndex, isWhite, occupancy);
    
    // Check for valid normal moves:
    // 1. Capture: Must be in attack set AND destination must be occupied
    // 2. Push: Must be in push set (pushes already accounts for blockers)
    const bool isCapture = (attacks & toBit) && ((occupancy & toBit) != 0ULL);
    const bool isPush = (pushes & toBit); // getPawnForwardPushes guarantees empty squares

    // Try normal pawn moves first (most common case)
    if (isCapture || isPush) [[likely]] {
        // King safety check for normal pawn moves
        uint64_t occNew = occupancy;
    occNew &= ~Board::bitMask(fromIndex);
    occNew |= toBit;
        
        const uint64_t excludeMask = (destPiece != EMPTY && destColor == oppColor) ? toBit : 0ULL;
        const uint64_t kingBB = kings_bb[side];
        if (!kingBB) [[unlikely]] return false;
        const uint8_t kingSq = __builtin_ctzll(kingBB);
        
        return !isKingAttackedCustom(kingSq, oppColor, occNew,
                                     pawns_bb[oppSide] & ~excludeMask,
                                     knights_bb[oppSide] & ~excludeMask,
                                     bishops_bb[oppSide] & ~excludeMask,
                                     rooks_bb[oppSide] & ~excludeMask,
                                     queens_bb[oppSide] & ~excludeMask,
                                     kings_bb[oppSide] & ~excludeMask);
    }
    
    // Try en passant if normal moves fail
    return isPawnEnPassantLegal(fromIndex, toIndex, movingColor);
}

// En passant move validation (extracted for clarity)
[[nodiscard]] inline bool Board::isPawnEnPassantLegal(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t movingColor
) const noexcept {
    // Early return: no en passant available
    if (!Coords::isInBounds(enPassant)) [[likely]] return false;
    
    // Early return: destination doesn't match en passant square
    if (toIndex != enPassant.index) [[likely]] return false;
    
    const bool isWhite = (movingColor == WHITE);
    const uint8_t side = colorToIndex(movingColor);
    const uint8_t oppSide = side ^ 1;
    const uint8_t oppColor = oppositeColor(movingColor);
    
    // Calculate captured pawn position
    const int8_t epDir = isWhite ? 8 : -8;
    const uint8_t capturedPawnIdx = static_cast<uint8_t>(toIndex + epDir);
    
    // Simulate en passant capture
    uint64_t occNew = occupancy;
    occNew &= ~Board::bitMask(fromIndex);
    occNew &= ~Board::bitMask(capturedPawnIdx);
    occNew |= Board::bitMask(toIndex);
    
    // King safety check
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return false;
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    
    return !isKingAttackedCustom(kingSq, oppColor, occNew,
                                 pawns_bb[oppSide] & ~Board::bitMask(capturedPawnIdx),
                                 knights_bb[oppSide],
                                 bishops_bb[oppSide],
                                 rooks_bb[oppSide],
                                 queens_bb[oppSide],
                                 kings_bb[oppSide]);
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
    if (fromIndex == toIndex) [[unlikely]] return false;

    const uint8_t oppColor = oppositeColor(movingColor);
    const int fileDelta = static_cast<int>(fileOf(toIndex)) - static_cast<int>(fileOf(fromIndex));
    const int rankDelta = static_cast<int>(rankOf(toIndex)) - static_cast<int>(rankOf(fromIndex));

    // Handle castling explicitly when king moves two files on same rank
    if (rankDelta == 0 && (fileDelta == 2 || fileDelta == -2)) {
        return canCastleToSquare(fromIndex, toIndex, movingColor);
    }

    // Normal king move: one-step king attack and destination not attacked
    const uint64_t attacks = pieces::KING_ATTACKS[fromIndex];
    if ((attacks & toBit) == 0ULL) return false;
    if (isSquareAttacked(toIndex, oppColor, fromIndex)) return false;

    return true;
}

// Castling dispatcher
[[nodiscard]] inline bool Board::canCastleToSquare(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t movingColor
) const noexcept {
    const uint8_t fromRank = rankOf(fromIndex);
    const uint8_t toRank = rankOf(toIndex);
    const uint8_t fromFile = fileOf(fromIndex);
    
    // Early returns: invalid castling conditions
    if (fromRank != toRank) return false;
    if (fromFile != 4) return false;
    
    const bool isWhite = (movingColor == WHITE);
    const uint8_t expectedRank = isWhite ? 7 : 0;
    
    if (fromRank != expectedRank) return false;
    
    // Calculate file delta
    const int df = static_cast<int>(fileOf(toIndex)) - 4;
    
    if (df == 2) return canCastleKingside(isWhite, fromIndex);
    if (df == -2) return canCastleQueenside(isWhite, fromIndex);
    
    return false;
}

// Generic castling validation (consolidated logic)
[[nodiscard]] inline bool Board::canCastleGeneric(
    bool isWhite,
    uint8_t fromIndex,
    bool isKingside
) const noexcept {
    const uint8_t side = colorBoolToIndex(isWhite);
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
    if (get(sq1) != EMPTY || get(sq2) != EMPTY) {
        return false;
    }
    
    if (!isKingside && get(fromIndex - 3) != EMPTY) {
        return false;
    }
    
    // Check rook presence
    if ((rooks_bb[side] & Board::bitMask(rookIdx)) == 0ULL) {
        return false;
    }
    
    // Check castle path safety
    const uint64_t castlePath = Board::bitMask(fromIndex) | Board::bitMask(sq1) | Board::bitMask(sq2);
    return isCastlePathSafe(castlePath, oppColor);
}

// Kingside castling validation
[[nodiscard]] inline bool Board::canCastleKingside(bool isWhite, uint8_t fromIndex) const noexcept {
    return canCastleGeneric(isWhite, fromIndex, true);
}

// Queenside castling validation
[[nodiscard]] inline bool Board::canCastleQueenside(bool isWhite, uint8_t fromIndex) const noexcept {
    return canCastleGeneric(isWhite, fromIndex, false);
}

// King safety check for non-king, non-pawn pieces
[[nodiscard]] inline bool Board::verifyKingSafetyForSimplePiece(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t movingColor,
    uint8_t destPiece,
    uint8_t destColor
) const noexcept {
    const uint8_t side = colorToIndex(movingColor);
    
    // Early return: no king found (should never happen)
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return false;
    
    const uint8_t oppSide = side ^ 1;
    const uint8_t oppColor = oppositeColor(movingColor);
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    
    // Simulate move
    uint64_t occNew = occupancy;
    occNew &= ~Board::bitMask(fromIndex);
    occNew |= Board::bitMask(toIndex);
    
    // Exclusion mask for captured piece
    const uint64_t excludeMask = (destPiece != EMPTY && destColor == oppColor) 
    ? Board::bitMask(toIndex) 
        : 0ULL;
    
    // Zero-copy king safety check
    return !isKingAttackedCustom(kingSq, oppColor, occNew,
                                 pawns_bb[oppSide] & ~excludeMask,
                                 knights_bb[oppSide] & ~excludeMask,
                                 bishops_bb[oppSide] & ~excludeMask,
                                 rooks_bb[oppSide] & ~excludeMask,
                                 queens_bb[oppSide] & ~excludeMask,
                                 kings_bb[oppSide] & ~excludeMask);
}

// ------------------------------------------------------------
// CHECK / CHECKMATE / STALEMATE UTILITIES
// ------------------------------------------------------------
// Returns true if square 'targetIndex' is attacked by 'byColor'
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept {
    const int side = colorToIndex(byColor);

    // Fast path: check non-sliding pieces first (cheaper)
    if (pieces::PAWN_ATTACKERS_TO[side][targetIndex] & pawns_bb[side]) return true;
    if (pieces::KNIGHT_ATTACKS[targetIndex] & knights_bb[side]) return true;
    if (pieces::KING_ATTACKS[targetIndex] & kings_bb[side]) return true;

    // Early exit: if no sliding pieces of this color, no attack possible
    if (!(rooks_bb[side] | bishops_bb[side] | queens_bb[side])) return false;

    // Sliding pieces check (expensive)
    const uint64_t rookMask   = pieces::getRookAttacks(targetIndex, occupancy);
    const uint64_t bishopMask = pieces::getBishopAttacks(targetIndex, occupancy);

    return ((rooks_bb[side] | queens_bb[side]) & rookMask)
         | ((bishops_bb[side] | queens_bb[side]) & bishopMask);
}


// Version that excludes a square from occupancy - useful for king moves
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept {
    const int side = colorToIndex(byColor);

    // Fast path: check non-sliding pieces first
    if (pieces::PAWN_ATTACKERS_TO[side][targetIndex] & pawns_bb[side]) return true;
    if (pieces::KNIGHT_ATTACKS[targetIndex] & knights_bb[side]) return true;
    if (pieces::KING_ATTACKS[targetIndex] & kings_bb[side]) return true;

    // Early exit: if no sliding pieces, no attack possible
    if (!(rooks_bb[side] | bishops_bb[side] | queens_bb[side])) return false;

    // Sliding pieces with modified occupancy
    const uint64_t occMinus = occupancy & ~Board::bitMask(excludeSquare);
    const uint64_t rookMask   = pieces::getRookAttacks(targetIndex, occMinus);
    const uint64_t bishopMask = pieces::getBishopAttacks(targetIndex, occMinus);

    return ((rooks_bb[side] | queens_bb[side]) & rookMask)
         | ((bishops_bb[side] | queens_bb[side]) & bishopMask);
}


// Optimized: check if ALL squares in mask are safe (not attacked by byColor)
// Returns true if all squares are safe, false if ANY square is attacked
// Used for castling to avoid 3 separate isSquareAttacked calls
bool Board::isCastlePathSafe(uint64_t squaresMask, uint8_t byColor) const noexcept {
    const int side = colorToIndex(byColor);
    
    // Check each square in the mask
    while (squaresMask) {
        const uint8_t sq = __builtin_ctzll(squaresMask);
        squaresMask &= squaresMask - 1; // Clear LSB
        
        // Early exit on first attacked square
        if (pieces::PAWN_ATTACKERS_TO[side][sq] & pawns_bb[side]) return false;
        if (pieces::KNIGHT_ATTACKS[sq] & knights_bb[side]) return false;
        if (pieces::KING_ATTACKS[sq] & kings_bb[side]) return false;
        
        const uint64_t rookMask   = pieces::getRookAttacks(sq, occupancy);
        const uint64_t bishopMask = pieces::getBishopAttacks(sq, occupancy);
        
        if (((rooks_bb[side] | queens_bb[side]) & rookMask) | 
            ((bishops_bb[side] | queens_bb[side]) & bishopMask)) {
            return false;
        }
    }
    
    return true; // All squares safe
}

// Helper: check if king at kingSq is attacked using custom bitboards
// Used internally to avoid code duplication when simulating moves
bool Board::isKingAttackedCustom(uint8_t kingSq, uint8_t byColor, uint64_t occ,
                                 uint64_t pawns, uint64_t knights, uint64_t bishops,
                                 uint64_t rooks, uint64_t queens, uint64_t kings) const noexcept {
    const uint8_t side = colorToIndex(byColor);
    
    // Fast path: non-sliding pieces
    if (pieces::PAWN_ATTACKERS_TO[side][kingSq] & pawns) return true;
    if (pieces::KNIGHT_ATTACKS[kingSq] & knights) return true;
    if (pieces::KING_ATTACKS[kingSq] & kings) return true;
    
    // Early exit: if no sliding pieces, no attack possible
    if (!(rooks | bishops | queens)) return false;
    
    // Sliding pieces
    if (pieces::getRookAttacks(kingSq, occ) & (rooks | queens)) return true;
    if (pieces::getBishopAttacks(kingSq, occ) & (bishops | queens)) return true;
    
    return false;
}

// Is the given color currently in check?
__attribute__((hot))
bool Board::inCheck(uint8_t color) const noexcept {
    // Find king square using king bitboards (convert to array index)
    const uint8_t side = colorToIndex(color);
    const uint64_t kingBB = kings_bb[side];

    if (!kingBB) [[unlikely]] return false; // no king found (invalid position) -> treat as not in check
    

    const uint8_t kingIndex = __builtin_ctzll(kingBB);
    const uint8_t opp = oppositeColor(color);
    return isSquareAttacked(kingIndex, opp);
}


// Helper template per simple pieces con pattern identico (Knight, Bishop, Rook, Queen)
// Elimina duplicazione codice in hasAnyLegalMove
template<uint8_t PieceType>
[[nodiscard]] static inline bool hasLegalMovesForPieceType(
    const Board* board,
    uint64_t pieceBB,
    uint64_t ownOcc,
    uint64_t occupancy,
    bool inCheck
) noexcept {
    while (pieceBB) {
        const uint8_t from = __builtin_ctzll(pieceBB);
        pieceBB &= pieceBB - 1;
        
        // Use dispatch table for move generation
        uint64_t movesMask = pieces::generateMovesByType<PieceType>(from, occupancy) & ~ownOcc;
        while (movesMask) {
            const uint8_t to = __builtin_ctzll(movesMask);
            movesMask &= movesMask - 1;
            if (board->canMoveToBB(Coords{from}, Coords{to}, inCheck)) return true;
        }
    }
    return false;
}


bool Board::hasAnyLegalMove(uint8_t color) const noexcept {
    const int side = (color == WHITE) ? 0 : 1;
    const int oppSide = side ^ 1;

    const bool inChk = inCheck(color);

    // Pre-calculate occupancy masks
    const uint64_t ownOcc = pawns_bb[side] | knights_bb[side] | bishops_bb[side] |
                             rooks_bb[side] | queens_bb[side]  | kings_bb[side];
    const uint64_t enemyOcc = pawns_bb[oppSide] | knights_bb[oppSide] | bishops_bb[oppSide] |
                               rooks_bb[oppSide] | queens_bb[oppSide]  | kings_bb[oppSide];

    // --- KING MOVES (check for king first - always exists, cheap to test) ---
    uint64_t kings = kings_bb[side];
    if (kings) [[likely]] {
        const uint8_t king = __builtin_ctzll(kings);
        uint64_t moves = pieces::KING_ATTACKS[king] & ~ownOcc;
        while (moves) {
            const uint8_t to = __builtin_ctzll(moves);
            moves &= moves - 1;
            if (canMoveToBB(Coords{king}, Coords{to}, inChk)) return true;
        }
        
        // Castling (only if not in check)
        if (!inChk) {
            const uint8_t eIndex = (side == 0) ? WHITE_KING_START : BLACK_KING_START;
            if (king == eIndex) {
                if (canMoveToBB(Coords{eIndex}, Coords{static_cast<uint8_t>(eIndex + 2)}, inChk)) return true;
                if (canMoveToBB(Coords{eIndex}, Coords{static_cast<uint8_t>(eIndex - 2)}, inChk)) return true;
            }
        }
    }

    // --- KNIGHTS (cheap, no magic bitboards) ---
    if (hasLegalMovesForPieceType<0x2>(this, knights_bb[side], ownOcc, occupancy, inChk)) {
        return true;
    }

    // --- PAWNS (most common pieces, optimized loop) ---
    const bool isWhite = (side == 0);
    uint64_t pawns = pawns_bb[side];
    while (pawns) {
        const uint8_t from = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        // Forward pushes (more common than captures)
        uint64_t push = pieces::getPawnForwardPushes(from, isWhite, occupancy);
        while (push) {
            const uint8_t to = __builtin_ctzll(push);
            push &= push - 1;
            if (canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }

        // Pawn captures
        uint64_t caps = pieces::PAWN_ATTACKS[isWhite][from] & enemyOcc;
        while (caps) {
            const uint8_t to = __builtin_ctzll(caps);
            caps &= caps - 1;
            if (canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }
    }

    // --- BISHOPS ---
    if (hasLegalMovesForPieceType<0x3>(this, bishops_bb[side], ownOcc, occupancy, inChk)) {
        return true;
    }

    // --- ROOKS ---
    if (hasLegalMovesForPieceType<0x4>(this, rooks_bb[side], ownOcc, occupancy, inChk)) {
        return true;
    }

    // --- QUEENS ---
    if (hasLegalMovesForPieceType<0x5>(this, queens_bb[side], ownOcc, occupancy, inChk)) {
        return true;
    }

    return false;
}

void Board::rebuildRepetitionHistory() noexcept {
    currentHash = zobrist::computeHashKey(*this);
    historySize = 0;
    repetitionHistory[historySize++] = currentHash;
}

void Board::updateRepetitionAfterMove(bool resetHistory) noexcept {
    currentHash = zobrist::computeHashKey(*this);
    if (resetHistory) {
        historySize = 0;
    }
    if (historySize >= repetitionHistory.size()) {
        historySize = static_cast<uint8_t>(repetitionHistory.size() - 1);
    }
    repetitionHistory[historySize++] = currentHash;
}

bool Board::isThreefoldRepetition() const noexcept {
    if (historySize == 0) return false;
    const uint64_t target = currentHash;
    int count = 0;
    for (uint8_t i = historySize; i > 0; --i) {
        if (repetitionHistory[i - 1] == target) {
            if (++count >= 3) return true;
        }
    }
    return false;
}

}; // namespace chess
