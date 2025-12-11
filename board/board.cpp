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
            // Coords convention: white moves toward rank 0, black toward rank 7
            const int8_t forwardDir = isWhite ? -1 : 1;
            // Captured pawn is one rank behind the destination
            const uint8_t capturedIndex = toIndex + (forwardDir << 3); // forwardDir * 8
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
    const uint64_t occ = this->occupancy;

    // Pre-calculate destination piece info (used multiple times)
    const uint8_t destPiece = this->get(to);
    const uint8_t destType = destPiece & MASK_PIECE_TYPE;
    const uint8_t destColor = destPiece & MASK_COLOR;

    // Early exit: can't capture own piece
    if (destPiece != EMPTY && destColor == movingColor) return false;

    // Detect check state and attackers for restrictions (double check logic)
    uint8_t attackerCount = 0;
    uint8_t kingIndex = 64; // invalid index
    if (inChk) {
        // Use the king bitboard to find king index quickly
        const uint8_t side = (movingColor == WHITE) ? 0 : 1;
        const uint64_t kingBB = kings_bb[side]; // Array index (0=WHITE, 1=BLACK)

        if (kingBB) [[likely]] {
            kingIndex = __builtin_ctzll(kingBB); // Count trailing zeros = find LSB

            const uint8_t oppSide = (oppColor == WHITE) ? 0 : 1; // Convert to array index
            
            // Pawns
            attackerCount += __builtin_popcountll(pieces::PAWN_ATTACKERS_TO[oppSide][kingIndex] & pawns_bb[oppSide]);
            
            // Knights
            attackerCount += __builtin_popcountll(pieces::KNIGHT_ATTACKS[kingIndex] & knights_bb[oppSide]);

            // Kings (adjacent)
            attackerCount += __builtin_popcountll(pieces::KING_ATTACKS[kingIndex] & kings_bb[oppSide]);

            // Sliding rook/queen (orthogonal)
            attackerCount += __builtin_popcountll((pieces::getRookAttacks(kingIndex, occ)) & 
                                                  (rooks_bb[oppSide] | queens_bb[oppSide]));
            // Sliding bishop/queen (diagonal)
            attackerCount += __builtin_popcountll((pieces::getBishopAttacks(kingIndex, occ)) & 
                                                  (bishops_bb[oppSide] | queens_bb[oppSide]));
        }
    }

    // Double check: only king moves allowed
    if (inChk && attackerCount >= 2 && fromType != KING) return false;
    

    switch (fromType) { // piece type only
        case PAWN: {
            const bool isWhite = (this->getColor(from) == WHITE);
            const uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][fromIndex];
            const uint64_t pushes  = pieces::getPawnForwardPushes(fromIndex, isWhite, occ);
            bool legal = false;
            bool isEnPassant = false;
            // En passant: diagonal into empty square matching enPassant target
            if ((attacks & toBit) && ((occ & toBit) == 0ULL)) {
                if (Coords::isInBounds(enPassant[0]) && toIndex == enPassant[0].index) {
                    legal = true;
                    isEnPassant = true;
                }
            }
            // Diagonal captures (must be occupied)
            if (!legal && (attacks & toBit) && ((occ & toBit) != 0ULL)) {
                legal = true;
            }
            // Forward pushes (must be empty)
            if (!legal && (pushes & toBit) && ((occ & toBit) == 0ULL)) {
                legal = true;
            }
            if (!legal) return false;

            // Simulate pawn move (including en-passant) using bitboards/occupancy only
            // to ensure king safety without copying the entire Board.
            const uint8_t side = (movingColor == WHITE) ? 0 : 1; // Convert to array index
            const uint8_t oppSide = side ^ 1;

            // New occupancy after the move
            uint64_t occNew = occ;
            const uint64_t fromMask = (1ULL << fromIndex);
            const uint64_t toMask   = (1ULL << toIndex);
            occNew &= ~fromMask;
            occNew |=  toMask;

            // Prepare opponent bitboards with captures applied
            uint64_t oppPawnsMask;
            uint64_t oppKnightsMask;
            uint64_t oppBishopsMask;
            uint64_t oppRooksMask;
            uint64_t oppQueensMask;
            uint64_t oppKingsMask;

            // Handle captures: normal capture on 'to' or en-passant captured pawn
            if (isEnPassant) {
                const bool isWhitePawn = isWhite;
                const int8_t dir = isWhitePawn ? 1 : -1;
                const uint8_t capIndex = toIndex + (dir << 3);
                const uint64_t capMask = (1ULL << capIndex);

                occNew &= ~capMask;
                
                // Only pawn bitboard changes in en passant
                oppPawnsMask   = pawns_bb[oppSide] & ~capMask;
                oppKnightsMask = knights_bb[oppSide];
                oppBishopsMask = bishops_bb[oppSide];
                oppRooksMask   = rooks_bb[oppSide];
                oppQueensMask  = queens_bb[oppSide];
                oppKingsMask   = kings_bb[oppSide];
            } else if (destPiece != EMPTY && destColor == oppColor) {
                // Normal capture: exclude captured piece from its bitboard
                oppPawnsMask   = (destType == PAWN)   ? (pawns_bb[oppSide] & ~toMask)   : pawns_bb[oppSide];
                oppKnightsMask = (destType == KNIGHT) ? (knights_bb[oppSide] & ~toMask) : knights_bb[oppSide];
                oppBishopsMask = (destType == BISHOP) ? (bishops_bb[oppSide] & ~toMask) : bishops_bb[oppSide];
                oppRooksMask   = (destType == ROOK)   ? (rooks_bb[oppSide] & ~toMask)   : rooks_bb[oppSide];
                oppQueensMask  = (destType == QUEEN)  ? (queens_bb[oppSide] & ~toMask)  : queens_bb[oppSide];
                oppKingsMask   = (destType == KING)   ? (kings_bb[oppSide] & ~toMask)   : kings_bb[oppSide];
            } else {
                // No capture: use bitboards directly
                oppPawnsMask   = pawns_bb[oppSide];
                oppKnightsMask = knights_bb[oppSide];
                oppBishopsMask = bishops_bb[oppSide];
                oppRooksMask   = rooks_bb[oppSide];
                oppQueensMask  = queens_bb[oppSide];
                oppKingsMask   = kings_bb[oppSide];
            }

            // King square (unchanged for pawn moves)
            const uint64_t kingBB = kings_bb[side]; // Array index
            if (!kingBB) [[unlikely]] return false; // invalid position: treat as illegal
            const uint8_t kingSq = __builtin_ctzll(kingBB);

            // Check if king is attacked in the new position
            const uint8_t oppSideForPawns = (oppColor == WHITE) ? 0 : 1; // Array index

            // Pawn attackers
            if (pieces::PAWN_ATTACKERS_TO[oppSideForPawns][kingSq] & oppPawnsMask) return false;

            // Knights
            if (pieces::KNIGHT_ATTACKS[kingSq] & oppKnightsMask) return false;

            // Kings (adjacent)
            if (pieces::KING_ATTACKS[kingSq] & oppKingsMask) return false;

            // Sliding rook/queen (orthogonal)
            if (pieces::getRookAttacks(kingSq, occNew) & (oppRooksMask | oppQueensMask)) return false;

            // Sliding bishop/queen (diagonal)
            if (pieces::getBishopAttacks(kingSq, occNew) & (oppBishopsMask | oppQueensMask)) return false;

            return true;
        }
        case KNIGHT:
            bitMap = pieces::KNIGHT_ATTACKS[fromIndex]; break;
        case BISHOP:
            bitMap = pieces::getBishopAttacks(fromIndex, occ); break;
        case ROOK:
            bitMap = pieces::getRookAttacks(fromIndex, occ); break;
        case QUEEN:
            bitMap = pieces::getQueenAttacks(fromIndex, occ); break;
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
        uint64_t occNew = occ;
        occNew &= ~fromMask;
        occNew |=  toMask;

        // King square (unchanged because king is not moving here)
        const uint64_t kingBB = kings_bb[side];
        if (!kingBB) [[unlikely]] return false;
        const uint8_t kingSq = __builtin_ctzll(kingBB);

        // Pre-calculate opponent bitboards with conditional masking (-10 branches)
        const bool isCapture = (destPiece != EMPTY && destColor == oppColor);
        
        // If capturing, exclude the captured piece from its bitboard
        const uint64_t oppPawnsMask   = (isCapture && destType == PAWN)   ? (pawns_bb[oppSide] & ~toMask)   : pawns_bb[oppSide];
        const uint64_t oppKnightsMask = (isCapture && destType == KNIGHT) ? (knights_bb[oppSide] & ~toMask) : knights_bb[oppSide];
        const uint64_t oppKingsMask   = (isCapture && destType == KING)   ? (kings_bb[oppSide] & ~toMask)   : kings_bb[oppSide];
        const uint64_t oppRooksMask   = (isCapture && destType == ROOK)   ? (rooks_bb[oppSide] & ~toMask)   : rooks_bb[oppSide];
        const uint64_t oppBishopsMask = (isCapture && destType == BISHOP) ? (bishops_bb[oppSide] & ~toMask) : bishops_bb[oppSide];
        const uint64_t oppQueensMask  = (isCapture && destType == QUEEN)  ? (queens_bb[oppSide] & ~toMask)  : queens_bb[oppSide];
        
        // Check if king is attacked in the new position (single branch per piece type)
        const uint8_t oppSideForPawns = (oppColor == WHITE) ? 0 : 1;
        
        if (pieces::PAWN_ATTACKERS_TO[oppSideForPawns][kingSq] & oppPawnsMask) return false;
        if (pieces::KNIGHT_ATTACKS[kingSq] & oppKnightsMask) return false;
        if (pieces::KING_ATTACKS[kingSq] & oppKingsMask) return false;
        
        // Sliding pieces: combine rooks+queens, bishops+queens
        if (pieces::getRookAttacks(kingSq, occNew) & (oppRooksMask | oppQueensMask)) return false;
        if (pieces::getBishopAttacks(kingSq, occNew) & (oppBishopsMask | oppQueensMask)) return false;
    }

    return true;
}

// ------------------------------------------------------------
// CHECK / CHECKMATE / STALEMATE UTILITIES
// ------------------------------------------------------------
// Returns true if square 'targetIndex' is attacked by 'byColor'
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept {
    const uint64_t occ = occupancy;
    const int side = (byColor == WHITE) ? 0 : 1;

    // Pawns: raramente attaccano, hint branch predictor
    if (pieces::PAWN_ATTACKERS_TO[side][targetIndex] & pawns_bb[side]) return true;

    // Knights
    if (pieces::KNIGHT_ATTACKS[targetIndex] & knights_bb[side]) return true;

    // Kings
    if (pieces::KING_ATTACKS[targetIndex] & kings_bb[side]) return true;

    // Sliding pieces combined check
    const uint64_t rookMask   = pieces::getRookAttacks(targetIndex, occ);
    const uint64_t bishopMask = pieces::getBishopAttacks(targetIndex, occ);

    const uint64_t slidingAttackers = ((rooks_bb[side] | queens_bb[side]) & rookMask)
                                    | ((bishops_bb[side] | queens_bb[side]) & bishopMask);

    return slidingAttackers;
}


// Version that excludes a square from occupancy - useful for king moves
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept {
    const uint64_t occMinus = occupancy & ~(1ULL << excludeSquare);
    const int side = (byColor == WHITE) ? 0 : 1;

    // Early exit: pawn attacks
    if (pieces::PAWN_ATTACKERS_TO[side][targetIndex] & pawns_bb[side]) return true;

    // Knight attacks
    if (pieces::KNIGHT_ATTACKS[targetIndex] & knights_bb[side]) return true;

    // King attacks
    if (pieces::KING_ATTACKS[targetIndex] & kings_bb[side]) return true;

    // Sliding pieces combined check
    const uint64_t rookMask   = pieces::getRookAttacks(targetIndex, occMinus);
    const uint64_t bishopMask = pieces::getBishopAttacks(targetIndex, occMinus);

    const uint64_t slidingAttackers = ((rooks_bb[side] | queens_bb[side]) & rookMask)
                                    | ((bishops_bb[side] | queens_bb[side]) & bishopMask);

    return slidingAttackers;
}


// Optimized: check if ALL squares in mask are safe (not attacked by byColor)
// Returns true if all squares are safe, false if ANY square is attacked
// Used for castling to avoid 3 separate isSquareAttacked calls
bool Board::isCastlePathSafe(uint64_t squaresMask, uint8_t byColor) const noexcept {
    const uint64_t occ = occupancy;
    const int side = (byColor == WHITE) ? 0 : 1;
    
    // Check each square in the mask
    while (squaresMask) {
        const uint8_t sq = __builtin_ctzll(squaresMask);
        squaresMask &= squaresMask - 1; // Clear LSB
        
        // Early exit on first attacked square
        if (pieces::PAWN_ATTACKERS_TO[side][sq] & pawns_bb[side]) return false;
        if (pieces::KNIGHT_ATTACKS[sq] & knights_bb[side]) return false;
        if (pieces::KING_ATTACKS[sq] & kings_bb[side]) return false;
        
        const uint64_t rookMask   = pieces::getRookAttacks(sq, occ);
        const uint64_t bishopMask = pieces::getBishopAttacks(sq, occ);
        
        if (((rooks_bb[side] | queens_bb[side]) & rookMask) | 
            ((bishops_bb[side] | queens_bb[side]) & bishopMask)) {
            return false;
        }
    }
    
    return true; // All squares safe
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

}; // namespace chess
