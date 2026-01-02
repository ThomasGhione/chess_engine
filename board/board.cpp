#include "board.hpp"
#ifdef DEBUG
#include <iostream>
#endif

namespace chess {


bool Board::moveBB(const Coords& from, const Coords& to) noexcept {   
    const uint8_t moving = this->get(from);
    const uint8_t movingColor = moving & this->MASK_COLOR;
    const bool inCheck = this->inCheck(movingColor);
    
    if (!canMoveToBB(from, to, inCheck)) return false;

    const uint8_t fromIndex = from.index;
    const uint8_t toIndex = to.index;
    const uint8_t movingType = moving & this->MASK_PIECE_TYPE;
    const uint8_t destBefore = this->get(to);

    // Clear en passant by default; may set again after a double push
    Coords prevEp = enPassant[0];
    enPassant[0] = Coords{};
    enPassant[1] = Coords{};

    // Handle en passant capture: pawn moves diagonally into empty ep square
    if (movingType == PAWN) {
        // Check if pawn moved diagonally (different file): (fromIndex & 7) != (toIndex & 7)
        if ((fromIndex & 7) != (toIndex & 7) && destBefore == EMPTY && Coords::isInBounds(prevEp) && (toIndex == prevEp.index)) {
            const bool isWhite = (movingColor == WHITE);
            // Coords convention: index increases with rank (a8=0 -> h1=63)
            // White moves toward rank 0 (decreasing), Black toward rank 7 (increasing)
            // Captured pawn is one rank "behind" where the capturing pawn lands
            // For White capturing: captured pawn is at higher rank (toIndex + 8)
            // For Black capturing: captured pawn is at lower rank (toIndex - 8)
            const int8_t captureOffset = isWhite ? 8 : -8;
            const uint8_t capturedIndex = toIndex + captureOffset;
            this->set(capturedIndex, EMPTY);
            this->occupancy &= ~(1ULL << capturedIndex);
        }
    }

    // Move the piece
    this->updateChessboard(from, to);
    this->fastUpdateOccupancyBB(fromIndex, toIndex);

    // Ensure per-piece bitboards are updated to reflect the move (including EP/rook changes handled above)
    this->updateOccupancyBB();

    // Castling rook move if king moved two squares on same rank
    // Check if same rank: (fromIndex >> 3) == (toIndex >> 3)
    if (movingType == KING && (fromIndex >> 3) == (toIndex >> 3)) {
        const int df = static_cast<int>(toIndex & 7) - static_cast<int>(fromIndex & 7);
        if (df == 2) {
            // kingside: rook h -> f
            const uint8_t rookFromIndex = toIndex + 1; // h = f + 2, simplified to toIndex + 1
            const uint8_t rookToIndex = toIndex - 1;   // f = g - 1, simplified to toIndex - 1
            const uint8_t rook = this->get(rookFromIndex);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                this->set(rookToIndex, static_cast<piece_id>(rook));
                this->set(rookFromIndex, EMPTY);
                this->occupancy |= (1ULL << rookToIndex);
                this->occupancy &= ~(1ULL << rookFromIndex);
            }
        } else if (df == -2) {
            // queenside: rook a -> d
            const uint8_t rookFromIndex = toIndex - 2; // a = c - 2, simplified to toIndex - 2
            const uint8_t rookToIndex = toIndex + 1;   // d = c + 1, simplified to toIndex + 1
            const uint8_t rook = this->get(rookFromIndex);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                this->set(rookToIndex, static_cast<piece_id>(rook));
                this->set(rookFromIndex, EMPTY);
                this->occupancy |= (1ULL << rookToIndex);
                this->occupancy &= ~(1ULL << rookFromIndex);
            }
        }
    }

    // Update castling rights for king/rook moves or rook captures on original squares
    auto disableWhiteKingside  = [&]{ castle &= static_cast<uint8_t>(~(1u << 0)); };
    auto disableWhiteQueenside = [&]{ castle &= static_cast<uint8_t>(~(1u << 1)); };
    auto disableBlackKingside  = [&]{ castle &= static_cast<uint8_t>(~(1u << 2)); };
    auto disableBlackQueenside = [&]{ castle &= static_cast<uint8_t>(~(1u << 3)); };

    if (movingType == KING) {
        if (movingColor == WHITE) { 
            disableWhiteKingside(); 
            disableWhiteQueenside(); 
            hasMoved |= (1u << 0); // white king
        }
        else { 
            disableBlackKingside(); 
            disableBlackQueenside(); 
            hasMoved |= (1u << 3); // black king
        }
    }
    if (movingType == ROOK) {
        if (movingColor == WHITE) {
            // White rooks at rank 7 (row 1): a1 = index 56, h1 = index 63
            if (fromIndex == 56) { // a1
                disableWhiteQueenside();
                hasMoved |= (1u << 1);
            }
            if (fromIndex == 63) { // h1
                disableWhiteKingside();
                hasMoved |= (1u << 2);
            }
        } else {
            // Black rooks at rank 0 (row 8): a8 = index 0, h8 = index 7
            if (fromIndex == 0) { // a8
                disableBlackQueenside();
                hasMoved |= (1u << 4);
            }
            if (fromIndex == 7) { // h8
                disableBlackKingside();
                hasMoved |= (1u << 5);
            }
        }
    }
    // If a rook was captured on its starting square, disable that side's castling
    if (destBefore != EMPTY && ((destBefore & MASK_PIECE_TYPE) == ROOK)) {
        if ((destBefore & MASK_COLOR) == WHITE) {
            if (toIndex == 56) disableWhiteQueenside(); // a1
            if (toIndex == 63) disableWhiteKingside();  // h1
        } else {
            if (toIndex == 0) disableBlackQueenside();  // a8
            if (toIndex == 7) disableBlackKingside();   // h8
        }
    }

    // Set en passant target if the move was a double pawn push
    if (movingType == PAWN) {
        // Optimize: use index arithmetic instead of rank() calls
        const int dr = static_cast<int>(toIndex >> 3) - static_cast<int>(fromIndex >> 3);
        if (dr == 2 || dr == -2) {
            const uint8_t midIndex = (fromIndex + toIndex) >> 1; // Average of indices
            enPassant[0] = Coords{midIndex};
        }
    }

    this->setNextTurn();
    return true;
}

// Promote a pawn at 'at' using the provided choice char: 'q','r','b','n' (case-insensitive).
// Returns false if the piece is not a pawn on its promotion rank, otherwise promotes and returns true.
bool Board::promote(const Coords& at, char choice) noexcept {
    const uint8_t piece = this->get(at);
    const uint8_t type = piece & MASK_PIECE_TYPE;
    if (type != PAWN) return false; // must be a pawn
    const uint8_t color = piece & MASK_COLOR; // BLACK if set, otherwise WHITE
    // Verify pawn is on last rank according to color
    // White promotes at rank 0 (row 8), Black promotes at rank 7 (row 1)
    const uint8_t rank = at.index >> 3; // Extract rank from index directly
    if ((color == WHITE && rank != 0) || (color == BLACK && rank != 7)) return false;

    choice = static_cast<char>(std::tolower(static_cast<unsigned char>(choice)));
    uint8_t newType = QUEEN; // default promotion
    switch (choice) {
        case 'q': newType = QUEEN;  break;
        case 'r': newType = ROOK;   break;
        case 'b': newType = BISHOP; break;
        case 'n': newType = KNIGHT; break;
        default: /* default to queen */ break;
    }

    this->set(at, static_cast<piece_id>(newType | color));
    // Occupancy remains unchanged (piece stays on the same square)
    return true;
}

// Overload: execute move and, if a pawn reaches last rank, promote using provided choice
bool Board::moveBB(const Coords& from, const Coords& to, char promotionChoice) noexcept {
    // Capture piece info before moving
    const uint8_t fromPiece = this->get(from);
    const uint8_t fromType = fromPiece & this->MASK_PIECE_TYPE;
    const uint8_t fromColor = fromPiece & this->MASK_COLOR;

    if (!this->moveBB(from, to)) {
        return false;
    }

    // If it was a pawn and landed on last rank, promote with given choice
    if (fromType == PAWN) {
        // White promotes at rank 0 (row 8), Black promotes at rank 7 (row 1)
        const uint8_t toRank = to.index >> 3; // Extract rank from index
        if ((fromColor == WHITE && toRank == 0) || (fromColor == BLACK && toRank == 7)) {
            (void)this->promote(to, promotionChoice);
        }
    }
    return true;
}

bool Board::canMoveToBB(const Coords& from, const Coords& to, bool inChk) const noexcept {
    uint64_t bitMap = 0ULL;

    const uint8_t fromType = this->get(from) & this->MASK_PIECE_TYPE;
    const uint8_t movingColor = this->getColor(from);
    const uint8_t oppColor = (movingColor == WHITE) ? BLACK : WHITE;

    const uint8_t fromIndex = from.index;
    const uint8_t toIndex = to.index;
    
    const uint64_t toBit = (1ULL << toIndex);

    // Pre-calculate destination piece info (used multiple times)
    const uint8_t destPiece = this->get(to);
    const uint8_t destType = destPiece & MASK_PIECE_TYPE;
    const uint8_t destColor = destPiece & MASK_COLOR;

    // Early exit: can't capture own piece
    if (destPiece != EMPTY && destColor == movingColor) return false;

    if (inChk) {
        uint8_t kingIndex = 64; // invalid index
        uint64_t attackerCount = 0; // Detect check state and attackers for restrictions (double check logic)

        // Use the king bitboard to find king index quickly
        const uint8_t side = (movingColor == WHITE) ? 0 : 1;

        kingIndex = static_cast<uint8_t>(__builtin_ctzll(kings_bb[side])); // Count trailing zeros = find LSB

        const uint8_t oppSide = (oppColor == WHITE) ? 0 : 1; // Convert to array index
        
        // Pawns
        attackerCount += __builtin_popcountll(pieces::PAWN_ATTACKERS_TO[oppSide][kingIndex] & pawns_bb[oppSide]);
        
        // Knights
        attackerCount += __builtin_popcountll(pieces::KNIGHT_ATTACKS[kingIndex] & knights_bb[oppSide]);

        // Kings (adjacent)
        attackerCount += __builtin_popcountll(pieces::KING_ATTACKS[kingIndex] & kings_bb[oppSide]);

        // Sliding rook/queen (orthogonal)
        attackerCount += __builtin_popcountll((pieces::getRookAttacks(kingIndex, occupancy)) & 
                                                (rooks_bb[oppSide] | queens_bb[oppSide]));
        // Sliding bishop/queen (diagonal)
        attackerCount += __builtin_popcountll((pieces::getBishopAttacks(kingIndex, occupancy)) & 
                                                (bishops_bb[oppSide] | queens_bb[oppSide]));
        
        // Double check: only king moves allowed
        if (attackerCount >= 2 && fromType != KING) [[unlikely]] return false;
    }

    
    switch (fromType) { // piece type only
        case PAWN: {
            const bool isWhite = movingColor == WHITE;
            
            const uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][fromIndex];
            const uint64_t pushes  = pieces::getPawnForwardPushes(fromIndex, isWhite, occupancy);
            bool legal = false;
            bool isEnPassant = false;
            
            // En passant: diagonal into empty square matching enPassant target
            if ((attacks & toBit) && ((occupancy & toBit) == 0ULL)) {
                if (Coords::isInBounds(enPassant[0]) && toIndex == enPassant[0].index) {
                    legal = true;
                    isEnPassant = true;
                }
            }
            // Diagonal captures (must be occupied)
            if (!legal) {
                if ((attacks & toBit) && ((occupancy & toBit) != 0ULL)) {
                    legal = true;
                }
                // Forward pushes (must be empty)
                if ((pushes & toBit) && ((occupancy & toBit) == 0ULL)) {
                    legal = true;
                }
                if (!legal) return false;
            }

            // Simulate pawn move (including en-passant) using bitboards/occupancy only
            // to ensure king safety without copying the entire Board.
            const uint8_t side = (movingColor == WHITE) ? 0 : 1; // Convert to array index
            const uint8_t oppSide = side ^ 1;

            // New occupancy after the move
            uint64_t occNew = occupancy;
            const uint64_t fromMask = (1ULL << fromIndex);
            const uint64_t toMask   = (1ULL << toIndex);
            occNew &= ~fromMask;
            occNew |=  toMask;

            // Optimized: compute only the exclusion mask for captured pieces
            uint64_t excludeMask = 0ULL;

            // Handle captures: normal capture on 'to' or en-passant captured pawn
            if (isEnPassant) {
                // Coords convention: index increases with rank (a8=0 -> h1=63)
                // White captures en passant: captured pawn is at higher rank (toIndex + 8)
                // Black captures en passant: captured pawn is at lower rank (toIndex - 8)
                const int8_t captureOffset = isWhite ? 8 : -8;
                const uint8_t capIndex = toIndex + captureOffset;
                excludeMask = (1ULL << capIndex);
                occNew &= ~excludeMask;
            } else if (destPiece != EMPTY && destColor == oppColor) {
                // Normal capture: exclude captured piece
                excludeMask = toMask;
            }

            // King square (unchanged for pawn moves)
            const uint64_t kingBB = kings_bb[side];
            if (!kingBB) [[unlikely]] return false; // invalid position: treat as illegal
            const uint8_t kingSq = __builtin_ctzll(kingBB);

            // Check if king is attacked using existing bitboards + exclusion mask
            // Pass bitboards directly from class members - zero copies!
            if (isKingAttackedCustom(kingSq, oppColor, occNew,
                                     pawns_bb[oppSide] & ~excludeMask,
                                     knights_bb[oppSide] & ~excludeMask,
                                     bishops_bb[oppSide] & ~excludeMask,
                                     rooks_bb[oppSide] & ~excludeMask,
                                     queens_bb[oppSide] & ~excludeMask,
                                     kings_bb[oppSide] & ~excludeMask)) {
                return false;
            }

            return true;
        }
        case KNIGHT:
            bitMap = pieces::KNIGHT_ATTACKS[fromIndex]; break;
        case BISHOP:
            bitMap = pieces::getBishopAttacks(fromIndex, occupancy); break;
        case ROOK:
            bitMap = pieces::getRookAttacks(fromIndex, occupancy); break;
        case QUEEN:
            bitMap = pieces::getQueenAttacks(fromIndex, occupancy); break;
        case KING: {
            bitMap = pieces::KING_ATTACKS[fromIndex];
            // Disallow king moves into attacked destination squares
            if (fromIndex != toIndex) { // Optimize: direct index comparison
                const uint8_t oppColor = movingColor == WHITE ? BLACK : WHITE; // Toggle color
                // CRITICAL: usa excludeSquare per evitare ray-blocking del re stesso
                if ((bitMap & toBit) && isSquareAttacked(toIndex, oppColor, fromIndex)) {
                    // Even if it's a normal king move square, it's attacked; reject
                    // (Castling logic handled below after this block.)
                    // We don't early return yet if castling attempt because castling handled separately
                    // We'll clear the bit to force failure unless castling returns true later.
                    bitMap &= ~toBit;
                }
            }
            // Castling legality: check rights, path emptiness, and safe squares
            const uint8_t fromRank = fromIndex >> 3; // Extract rank
            const uint8_t toRank = toIndex >> 3;
            if (fromRank == toRank) { // Same rank check
                const bool isWhite = (movingColor == WHITE);
                const int df = static_cast<int>(toIndex & 7) - static_cast<int>(fromIndex & 7); // file diff
                const uint8_t kf = fromIndex & 7; // king file
                // Only allow castling from the initial king square (e1=rank 7 for white, e8=rank 0 for black)
                if (!((isWhite && fromRank == 7 && kf == 4) || (!isWhite && fromRank == 0 && kf == 4))) {
                    break;
                }
                if (df == 2) { // kingside
                    bool rights = isWhite
                        ? ((castle & (1u << 0)) != 0u) // white O-O
                        : ((castle & (1u << 2)) != 0u); // black O-O
                    // Optimize: calculate indices directly from rank and file offsets
                    const uint8_t f1Idx = fromIndex + 1; // kf+1
                    const uint8_t f2Idx = fromIndex + 2; // kf+2
                    const uint8_t rookIdx = fromIndex + 3; // kf+3
                    const bool emptyBetween = (this->get(f1Idx) == EMPTY) && (this->get(f2Idx) == EMPTY);
                    
                    // Use bitboard to check rook presence (faster than get + mask)
                    const uint8_t side = isWhite ? 0 : 1;
                    const bool rookOk = (rooks_bb[side] & (1ULL << rookIdx)) != 0;
                    
                    if (rights && emptyBetween && rookOk) {
                        const uint8_t opp = isWhite ? BLACK : WHITE;
                        // Check all 3 squares at once (fromIndex, f1Idx, f2Idx)
                        const uint64_t castlePath = (1ULL << fromIndex) | (1ULL << f1Idx) | (1ULL << f2Idx);
                        if (isCastlePathSafe(castlePath, opp)) return true;
                    }
                } else if (df == -2) { // queenside
                    const bool rights = isWhite
                        ? ((castle & (1u << 1)) != 0u) // white O-O-O
                        : ((castle & (1u << 3)) != 0u); // black O-O-O

                    // Optimize: calculate indices directly
                    const uint8_t d1Idx = fromIndex - 1; // kf-1
                    const uint8_t d2Idx = fromIndex - 2; // kf-2
                    const uint8_t d3Idx = fromIndex - 3; // kf-3
                    const uint8_t rookIdx = fromIndex - 4; // kf-4
                    const bool emptyBetween = (this->get(d1Idx) == EMPTY) && (this->get(d2Idx) == EMPTY) && (this->get(d3Idx) == EMPTY);

                    // Use bitboard to check rook presence
                    const uint8_t side = isWhite ? 0 : 1;
                    const bool rookOk = (rooks_bb[side] & (1ULL << rookIdx)) != 0;

                    if (rights && emptyBetween && rookOk) {
                        const uint8_t opp = isWhite ? BLACK : WHITE;
                        // Check all 3 squares at once (fromIndex, d1Idx, d2Idx)
                        const uint64_t castlePath = (1ULL << fromIndex) | (1ULL << d1Idx) | (1ULL << d2Idx);
                        if (isCastlePathSafe(castlePath, opp)) return true;
                    }
                }
            }
            break;
        }
        default: return false;
    }

    if ((bitMap & toBit) == 0ULL) return false;


    // For any non-king, non-pawn move, ensure king safety (pins and check resolution)
    if (fromType != KING && fromType != PAWN) {
        const uint8_t side = (movingColor == WHITE) ? 0 : 1;
        const uint8_t oppSide = side ^ 1;

        const uint64_t fromMask = (1ULL << fromIndex);
        const uint64_t toMask   = (1ULL << toIndex);

        // New occupancy after the move
        uint64_t occNew = occupancy;
        occNew &= ~fromMask;
        occNew |=  toMask;

        // King square (unchanged because king is not moving here)
        const uint64_t kingBB = kings_bb[side];
        if (!kingBB) [[unlikely]] return false;
        const uint8_t kingSq = __builtin_ctzll(kingBB);

        // Optimized: compute only the exclusion mask for captured piece (if any)
        const uint64_t excludeMask = (destPiece != EMPTY && destColor == oppColor) ? toMask : 0ULL;
        
        // Check if king is attacked using existing bitboards + exclusion mask
        // Pass bitboards directly from class members - zero copies!
        if (isKingAttackedCustom(kingSq, oppColor, occNew,
                                 pawns_bb[oppSide] & ~excludeMask,
                                 knights_bb[oppSide] & ~excludeMask,
                                 bishops_bb[oppSide] & ~excludeMask,
                                 rooks_bb[oppSide] & ~excludeMask,
                                 queens_bb[oppSide] & ~excludeMask,
                                 kings_bb[oppSide] & ~excludeMask)) {
            return false;
        }
    }

    return true;
}

// ------------------------------------------------------------
// CHECK / CHECKMATE / STALEMATE UTILITIES
// ------------------------------------------------------------
// Returns true if square 'targetIndex' is attacked by 'byColor'
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept {
    const int side = (byColor == WHITE) ? 0 : 1;

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
    const int side = (byColor == WHITE) ? 0 : 1;

    // Fast path: check non-sliding pieces first
    if (pieces::PAWN_ATTACKERS_TO[side][targetIndex] & pawns_bb[side]) return true;
    if (pieces::KNIGHT_ATTACKS[targetIndex] & knights_bb[side]) return true;
    if (pieces::KING_ATTACKS[targetIndex] & kings_bb[side]) return true;

    // Early exit: if no sliding pieces, no attack possible
    if (!(rooks_bb[side] | bishops_bb[side] | queens_bb[side])) return false;

    // Sliding pieces with modified occupancy
    const uint64_t occMinus = occupancy & ~(1ULL << excludeSquare);
    const uint64_t rookMask   = pieces::getRookAttacks(targetIndex, occMinus);
    const uint64_t bishopMask = pieces::getBishopAttacks(targetIndex, occMinus);

    return ((rooks_bb[side] | queens_bb[side]) & rookMask)
         | ((bishops_bb[side] | queens_bb[side]) & bishopMask);
}


// Optimized: check if ALL squares in mask are safe (not attacked by byColor)
// Returns true if all squares are safe, false if ANY square is attacked
// Used for castling to avoid 3 separate isSquareAttacked calls
bool Board::isCastlePathSafe(uint64_t squaresMask, uint8_t byColor) const noexcept {
    const int side = (byColor == WHITE) ? 0 : 1;
    
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
    const uint8_t side = (byColor == WHITE) ? 0 : 1;
    
    // Fast path: non-sliding pieces
    if (pieces::PAWN_ATTACKERS_TO[side][kingSq] & pawns) return true;
    if (pieces::KNIGHT_ATTACKS[kingSq] & knights) return true;
    if (pieces::KING_ATTACKS[kingSq] & kings) return true;
    
    // Early exit: if no sliding pieces, no attack possible
    if (!(rooks | bishops | queens)) [[likely]] return false;
    
    // Sliding pieces
    if (pieces::getRookAttacks(kingSq, occ) & (rooks | queens)) return true;
    if (pieces::getBishopAttacks(kingSq, occ) & (bishops | queens)) return true;
    
    return false;
}

// Is the given color currently in check?
bool Board::inCheck(uint8_t color) const noexcept {
    // Find king square using king bitboards (convert to array index)
    const uint8_t side = (color == WHITE) ? 0 : 1;
    const uint64_t kingBB = kings_bb[side];

    if (!kingBB) [[unlikely]] return false; // no king found (invalid position) -> treat as not in check
    

    const uint8_t kingIndex = __builtin_ctzll(kingBB);
    const uint8_t opp = (color == WHITE) ? BLACK : WHITE;
    return isSquareAttacked(kingIndex, opp);
}


bool Board::hasAnyLegalMove(uint8_t color) const noexcept {
    const int side = (color == WHITE) ? 0 : 1;
    const int oppSide = side ^ 1;

    const bool inChk = this->inCheck(color);

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
            if (this->canMoveToBB(Coords{king}, Coords{to}, inChk)) return true;
        }
        
        // Castling (only if not in check)
        if (!inChk) [[likely]] {
            const uint8_t eIndex = (side == 0) ? 60 : 4;
            if (king == eIndex) {
                if (this->canMoveToBB(Coords{eIndex}, Coords{static_cast<uint8_t>(eIndex + 2)}, inChk)) return true;
                if (this->canMoveToBB(Coords{eIndex}, Coords{static_cast<uint8_t>(eIndex - 2)}, inChk)) return true;
            }
        }
    }

    // --- KNIGHTS (cheap, no magic bitboards) ---
    uint64_t knights = knights_bb[side];
    while (knights) {
        const uint8_t from = __builtin_ctzll(knights);
        knights &= knights - 1;
        
        uint64_t movesMask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        while (movesMask) {
            const uint8_t to = __builtin_ctzll(movesMask);
            movesMask &= movesMask - 1;
            if (this->canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }
    }

    // --- PAWNS (most common pieces, optimized loop) ---
    const bool isWhite = (side == 0);
    uint64_t pawns = pawns_bb[side];
    while (pawns) {
        const uint8_t from = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        // Forward pushes (more common than captures)
        uint64_t push = pieces::getPawnForwardPushes(from, isWhite, this->occupancy);
        while (push) {
            const uint8_t to = __builtin_ctzll(push);
            push &= push - 1;
            if (this->canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }

        // Pawn captures
        uint64_t caps = pieces::PAWN_ATTACKS[isWhite][from] & enemyOcc;
        while (caps) {
            const uint8_t to = __builtin_ctzll(caps);
            caps &= caps - 1;
            if (this->canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }
    }

    // --- BISHOPS ---
    uint64_t bishops = bishops_bb[side];
    while (bishops) {
        const uint8_t from = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        
        uint64_t movesMask = pieces::getBishopAttacks(from, this->occupancy) & ~ownOcc;
        while (movesMask) {
            const uint8_t to = __builtin_ctzll(movesMask);
            movesMask &= movesMask - 1;
            if (this->canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }
    }

    // --- ROOKS ---
    uint64_t rooks = rooks_bb[side];
    while (rooks) {
        const uint8_t from = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        
        uint64_t movesMask = pieces::getRookAttacks(from, this->occupancy) & ~ownOcc;
        while (movesMask) {
            const uint8_t to = __builtin_ctzll(movesMask);
            movesMask &= movesMask - 1;
            if (this->canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }
    }

    // --- QUEENS ---
    uint64_t queens = queens_bb[side];
    while (queens) {
        const uint8_t from = __builtin_ctzll(queens);
        queens &= queens - 1;
        
        uint64_t movesMask = pieces::getQueenAttacks(from, this->occupancy) & ~ownOcc;
        while (movesMask) {
            const uint8_t to = __builtin_ctzll(movesMask);
            movesMask &= movesMask - 1;
            if (this->canMoveToBB(Coords{from}, Coords{to}, inChk)) return true;
        }
    }

    return false;
}

}; // namespace chess
