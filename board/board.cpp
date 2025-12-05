#include "board.hpp"

namespace chess {


bool Board::moveBB(const Coords& from, const Coords& to) noexcept {   
    if (!canMoveToBB(from, to)) return false;

    const uint8_t moving = this->get(from);
    const uint8_t movingType = moving & this->MASK_PIECE_TYPE;
    const uint8_t movingColor = moving & this->MASK_COLOR;
    const uint8_t destBefore = this->get(to);

    // Clear en passant by default; may set again after a double push
    Coords prevEp = enPassant[0];
    enPassant[0] = Coords{};
    enPassant[1] = Coords{};

    // Handle en passant capture: pawn moves diagonally into empty ep square
    if (movingType == PAWN) {
        if (from.file != to.file && destBefore == EMPTY && Coords::isInBounds(prevEp) && (to.toIndex() == prevEp.toIndex())) {
            const bool isWhite = (movingColor == WHITE);
            int8_t forwardDir = isWhite ? 1 : -1;
            Coords captured{to.file, static_cast<uint8_t>(to.rank - forwardDir)};
            this->set(captured, EMPTY);
            this->occupancy &= ~(1ULL << captured.toIndex());
        }
    }

    // Move the piece
    this->updateChessboard(from, to);
    this->fastUpdateOccupancyBB(from.toIndex(), to.toIndex());

    // Ensure per-piece bitboards are updated to reflect the move (including EP/rook changes handled above)
    this->updateOccupancyBB();

    // Castling rook move if king moved two squares on same rank
    if (movingType == KING && from.rank == to.rank) {
        int df = static_cast<int>(to.file) - static_cast<int>(from.file);
        if (df == 2) {
            // kingside: rook h -> f
            Coords rookFrom{7, to.rank};
            Coords rookTo{5, to.rank};
            uint8_t rook = this->get(rookFrom);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                this->set(rookTo, static_cast<piece_id>(rook));
                this->set(rookFrom, EMPTY);
                this->occupancy |= (1ULL << rookTo.toIndex());
                this->occupancy &= ~(1ULL << rookFrom.toIndex());
            }
        } else if (df == -2) {
            // queenside: rook a -> d
            Coords rookFrom{0, to.rank};
            Coords rookTo{3, to.rank};
            uint8_t rook = this->get(rookFrom);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                this->set(rookTo, static_cast<piece_id>(rook));
                this->set(rookFrom, EMPTY);
                this->occupancy |= (1ULL << rookTo.toIndex());
                this->occupancy &= ~(1ULL << rookFrom.toIndex());
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
            if (from.rank == 0 && from.file == 0) { 
                disableWhiteQueenside();
                hasMoved |= (1u << 1); // white a1 rook
            }
            if (from.rank == 0 && from.file == 7) { 
                disableWhiteKingside();
                hasMoved |= (1u << 2); // white h1 rook
            }
        } else {
            if (from.rank == 7 && from.file == 0) { 
                disableBlackQueenside();
                hasMoved |= (1u << 4); // black a8 rook
            }
            if (from.rank == 7 && from.file == 7) { 
                disableBlackKingside();
                hasMoved |= (1u << 5); // black h8 rook
            }
        }
    }
    // If a rook was captured on its starting square, disable that side's castling
    if (destBefore != EMPTY && ((destBefore & MASK_PIECE_TYPE) == ROOK)) {
        if ((destBefore & MASK_COLOR) == WHITE) {
            if (to.rank == 0 && to.file == 0) disableWhiteQueenside();
            if (to.rank == 0 && to.file == 7) disableWhiteKingside();
        } else {
            if (to.rank == 7 && to.file == 0) disableBlackQueenside();
            if (to.rank == 7 && to.file == 7) disableBlackKingside();
        }
    }

    // Set en passant target if the move was a double pawn push
    if (movingType == PAWN) {
        int dr = static_cast<int>(to.rank) - static_cast<int>(from.rank);
        if (dr == 2 || dr == -2) {
            uint8_t midRank = (from.rank + to.rank) / 2;
            enPassant[0] = Coords{from.file, midRank};
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
    if ((color == WHITE && at.rank != 7) || (color == BLACK && at.rank != 0)) return false;

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
        if ((fromColor == WHITE && to.rank == 7) || (fromColor == BLACK && to.rank == 0)) {
            (void)this->promote(to, promotionChoice);
        }
    }
    return true;
}

bool Board::canMoveToBB(const Coords& from, const Coords& to) const noexcept {
    uint64_t bitMap = 0ULL;

    const uint8_t fromType = this->get(from) & this->MASK_PIECE_TYPE;
    const uint8_t movingColor = this->getColor(from);
    const uint8_t oppColor = (movingColor == WHITE) ? BLACK : WHITE;

    const uint8_t fromIndex = from.toIndex();
    const uint8_t toIndex = to.toIndex();
    
    const uint64_t toBit = (1ULL << toIndex);
    const uint64_t occ = this->occupancy; // current board occupancy

    // Detect check state and attackers for restrictions (double check logic)
    bool inChk = this->inCheck(movingColor);
    uint8_t attackerCount = 0;
    uint8_t kingIndex = 64; // invalid sentinel
    if (inChk) {
        // Use the king bitboard to find king index quickly
        const uint64_t kingBB = (movingColor == WHITE) ? kings_bb[0] : kings_bb[1];
        if (kingBB) {
            kingIndex = __builtin_ctzll(kingBB);

            const bool oppIsWhite = (oppColor == WHITE);

            // Pawns attacking king square
            {
                uint64_t pawnsAtt = pieces::PAWN_ATTACKERS_TO[oppIsWhite][kingIndex] &
                                    (oppIsWhite ? pawns_bb[0] : pawns_bb[1]);
                attackerCount += __builtin_popcountll(pawnsAtt);
            }

            // Knights
            {
                uint64_t knightsAtt = pieces::KNIGHT_ATTACKS[kingIndex] &
                                      (oppIsWhite ? knights_bb[0] : knights_bb[1]);
                attackerCount += __builtin_popcountll(knightsAtt);
            }

            // Kings (adjacent)
            {
                uint64_t kingsAtt = pieces::KING_ATTACKS[kingIndex] &
                                    (oppIsWhite ? kings_bb[0] : kings_bb[1]);
                attackerCount += __builtin_popcountll(kingsAtt);
            }

            // Sliding rook/queen (orthogonal)
            {
                uint64_t rookMask = pieces::getRookAttacks(kingIndex, occ);
                uint64_t rq = oppIsWhite ? (rooks_bb[0] | queens_bb[0]) : (rooks_bb[1] | queens_bb[1]);
                uint64_t rookAtt = rookMask & rq;
                attackerCount += __builtin_popcountll(rookAtt);
            }

            // Sliding bishop/queen (diagonal)
            {
                uint64_t bishopMask = pieces::getBishopAttacks(kingIndex, occ);
                uint64_t bq = oppIsWhite ? (bishops_bb[0] | queens_bb[0]) : (bishops_bb[1] | queens_bb[1]);
                uint64_t bishopAtt = bishopMask & bq;
                attackerCount += __builtin_popcountll(bishopAtt);
            }
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
                if (Coords::isInBounds(enPassant[0]) && toIndex == enPassant[0].toIndex()) {
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
            const uint8_t side = (movingColor == WHITE) ? 0 : 1;
            const uint8_t oppSide = side ^ 1;

            // New occupancy after the move
            uint64_t occNew = occ;
            const uint64_t fromMask = (1ULL << fromIndex);
            const uint64_t toMask   = (1ULL << toIndex);
            occNew &= ~fromMask;
            occNew |=  toMask;

            // Simulate per-piece bitboards for both sides
            uint64_t ourPawns    = pawns_bb[side];
            // uint64_t ourKnights  = knights_bb[side];
            // uint64_t ourBishops  = bishops_bb[side];
            // uint64_t ourRooks    = rooks_bb[side];
            // uint64_t ourQueens   = queens_bb[side];
            // uint64_t ourKings    = kings_bb[side];

            uint64_t oppPawns    = pawns_bb[oppSide];
            uint64_t oppKnights  = knights_bb[oppSide];
            uint64_t oppBishops  = bishops_bb[oppSide];
            uint64_t oppRooks    = rooks_bb[oppSide];
            uint64_t oppQueens   = queens_bb[oppSide];
            uint64_t oppKings    = kings_bb[oppSide];

            // Move our pawn in its bitboard
            ourPawns &= ~fromMask;
            ourPawns |=  toMask;

            // Handle captures: normal capture on 'to' or en-passant captured pawn
            if (isEnPassant) {
                const bool isWhitePawn = isWhite;
                const int8_t dir = isWhitePawn ? 1 : -1;
                Coords captured{to.file, static_cast<uint8_t>(to.rank - dir)};
                const uint8_t capIndex = captured.toIndex();
                const uint64_t capMask = (1ULL << capIndex);

                // Remove captured enemy pawn from occupancy and its pawn bitboard
                occNew &= ~capMask;
                oppPawns &= ~capMask;
            } else {
                // Normal capture: if destination had enemy piece, remove it from its bitboard
                const uint8_t destPiece = this->get(to);
                if (destPiece != EMPTY) {
                    const uint8_t destColor = destPiece & MASK_COLOR;
                    if (destColor == oppColor) {
                        const uint8_t destType = destPiece & MASK_PIECE_TYPE;
                        switch (destType) {
                            case PAWN:   oppPawns   &= ~toMask; break;
                            case KNIGHT: oppKnights &= ~toMask; break;
                            case BISHOP: oppBishops &= ~toMask; break;
                            case ROOK:   oppRooks   &= ~toMask; break;
                            case QUEEN:  oppQueens  &= ~toMask; break;
                            case KING:   oppKings   &= ~toMask; break;
                            default: break;
                        }
                    }
                }
            }

            // King square (unchanged for pawn moves)
            const uint64_t kingBB = (movingColor == WHITE) ? kings_bb[0] : kings_bb[1];
            if (!kingBB) return false; // invalid position: treat as illegal
            const uint8_t kingSq = __builtin_ctzll(kingBB);

            // Check if king is attacked in the new position
            const bool oppIsWhite = (oppColor == WHITE);

            // Pawn attackers
            uint64_t pawnAttackers = pieces::PAWN_ATTACKERS_TO[oppIsWhite][static_cast<int16_t>(kingSq)];
            if (pawnAttackers & (oppIsWhite ? oppPawns : oppPawns)) return false;

            // Knights
            if (pieces::KNIGHT_ATTACKS[static_cast<int16_t>(kingSq)] & oppKnights) return false;

            // Kings (adjacent)
            if (pieces::KING_ATTACKS[static_cast<int16_t>(kingSq)] & oppKings) return false;

            // Sliding rook/queen (orthogonal)
            {
                uint64_t mask = pieces::getRookAttacks(static_cast<int16_t>(kingSq), occNew);
                uint64_t rq = oppRooks | oppQueens;
                if (mask & rq) return false;
            }

            // Sliding bishop/queen (diagonal)
            {
                uint64_t mask = pieces::getBishopAttacks(static_cast<int16_t>(kingSq), occNew);
                uint64_t bq = oppBishops | oppQueens;
                if (mask & bq) return false;
            }

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
            if (from.toIndex() != to.toIndex()) {
                const uint8_t oppColor = (this->getColor(from) == WHITE) ? BLACK : WHITE;
                if ((bitMap & toBit) && isSquareAttacked(toIndex, oppColor)) {
                    // Even if it's a normal king move square, it's attacked; reject
                    // (Castling logic handled below after this block.)
                    // We don't early return yet if castling attempt because castling handled separately
                    // We'll clear the bit to force failure unless castling returns true later.
                    bitMap &= ~toBit;
                }
            }
            // Castling legality: check rights, path emptiness, and safe squares
            if (from.rank == to.rank) {
                const bool isWhite = (this->getColor(from) == WHITE);
                int df = static_cast<int>(to.file) - static_cast<int>(from.file);
                const uint8_t r = from.rank;
                const uint8_t kf = from.file;
                // Only allow castling from the initial king square (e1/e8)
                if (!((isWhite && r == 0 && kf == 4) || (!isWhite && r == 7 && kf == 4))) {
                    break;
                }
                if (df == 2) { // kingside
                    bool rights = isWhite
                        ? ((castle & (1u << 0)) != 0u) // white O-O
                        : ((castle & (1u << 2)) != 0u); // black O-O
                    bool emptyBetween = (this->get(r, (kf + 1)) == EMPTY) && (this->get(r, (kf + 2)) == EMPTY);
                    {
                        uint8_t rookPiece = this->get(r, (kf + 3));
                        bool rookOk = ((rookPiece & MASK_PIECE_TYPE) == ROOK) && ((rookPiece & MASK_COLOR) == (isWhite ? WHITE : BLACK));
                        if (!rookOk) {
                            // fallthrough
                        } else {
                            uint8_t eIdx = (r * 8 + kf);
                            uint8_t fIdx = (r * 8 + (kf + 1));
                            uint8_t gIdx = (r * 8 + (kf + 2));
                            uint8_t opp = isWhite ? BLACK : WHITE;
                            bool safe = !isSquareAttacked(eIdx, opp) && !isSquareAttacked(fIdx, opp) && !isSquareAttacked(gIdx, opp);
                            if (rights && emptyBetween && rookOk && safe) return true;
                        }
                    }
                } else if (df == -2) { // queenside
                    bool rights = isWhite
                        ? ((castle & (1u << 1)) != 0u) // white O-O-O
                        : ((castle & (1u << 3)) != 0u); // black O-O-O
                    bool emptyBetween = (this->get(r, (kf - 1)) == EMPTY) && (this->get(r, (kf - 2)) == EMPTY) && (this->get(r, (kf - 3)) == EMPTY);
                    {
                        uint8_t rookPiece = this->get(r, (kf - 4));
                        bool rookOk = ((rookPiece & MASK_PIECE_TYPE) == ROOK) && ((rookPiece & MASK_COLOR) == (isWhite ? WHITE : BLACK));
                        uint8_t eIdx = (r * 8 + kf);
                        uint8_t dIdx = (r * 8 + (kf - 1));
                        uint8_t cIdx = (r * 8 + (kf - 2));
                        uint8_t opp = isWhite ? BLACK : WHITE;
                        bool safe = !isSquareAttacked(eIdx, opp) && !isSquareAttacked(dIdx, opp) && !isSquareAttacked(cIdx, opp);
                        if (rights && emptyBetween && rookOk && safe) return true;
                    }
                }
            }
            break;
        }
        default: return false;
    }

    if ((bitMap & toBit) == 0ULL) return false;

    // Destination must not contain a friendly piece (safety guard; usually filtered earlier).
    const uint8_t destPiece = this->get(to);
    if (destPiece != EMPTY && ((destPiece & MASK_COLOR) == movingColor)) {
        return false;
    }

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

        // Local copies of our and opponent bitboards
        // uint64_t ourPawns    = pawns_bb[side];
        uint64_t ourKnights  = knights_bb[side];
        uint64_t ourBishops  = bishops_bb[side];
        uint64_t ourRooks    = rooks_bb[side];
        uint64_t ourQueens   = queens_bb[side];
        // uint64_t ourKings    = kings_bb[side];

        uint64_t oppPawns    = pawns_bb[oppSide];
        uint64_t oppKnights  = knights_bb[oppSide];
        uint64_t oppBishops  = bishops_bb[oppSide];
        uint64_t oppRooks    = rooks_bb[oppSide];
        uint64_t oppQueens   = queens_bb[oppSide];
        uint64_t oppKings    = kings_bb[oppSide];

        // Move our piece in its corresponding bitboard
        switch (fromType) {
            case KNIGHT:
                ourKnights &= ~fromMask;
                ourKnights |=  toMask;
                break;
            case BISHOP:
                ourBishops &= ~fromMask;
                ourBishops |=  toMask;
                break;
            case ROOK:
                ourRooks &= ~fromMask;
                ourRooks |=  toMask;
                break;
            case QUEEN:
                ourQueens &= ~fromMask;
                ourQueens |=  toMask;
                break;
            default:
                break;
        }

        // If this is a capture, remove the captured enemy from its bitboard
        if (destPiece != EMPTY) {
            const uint8_t destColor = destPiece & MASK_COLOR;
            if (destColor == oppColor) {
                const uint8_t destType = destPiece & MASK_PIECE_TYPE;
                switch (destType) {
                    case PAWN:   oppPawns   &= ~toMask; break;
                    case KNIGHT: oppKnights &= ~toMask; break;
                    case BISHOP: oppBishops &= ~toMask; break;
                    case ROOK:   oppRooks   &= ~toMask; break;
                    case QUEEN:  oppQueens  &= ~toMask; break;
                    case KING:   oppKings   &= ~toMask; break;
                    default: break;
                }
            }
        }

        // King square (unchanged because king is not moving here)
        const uint64_t kingBB = (movingColor == WHITE) ? kings_bb[0] : kings_bb[1];
        if (!kingBB) return false; // invalid position: treat as illegal
        const uint8_t kingSq = __builtin_ctzll(kingBB);

        // Check if king is attacked in the new position using updated occupancy/bitboards
        const bool oppIsWhite = (oppColor == WHITE);

        // Pawn attackers
        uint64_t pawnAttackers = pieces::PAWN_ATTACKERS_TO[oppIsWhite][static_cast<int16_t>(kingSq)];
        if (pawnAttackers & (oppIsWhite ? oppPawns : oppPawns)) return false;

        // Knights
        if (pieces::KNIGHT_ATTACKS[static_cast<int16_t>(kingSq)] & oppKnights) return false;

        // Kings (adjacent)
        if (pieces::KING_ATTACKS[static_cast<int16_t>(kingSq)] & oppKings) return false;

        // Sliding rook/queen (orthogonal)
        {
            uint64_t mask = pieces::getRookAttacks(static_cast<int16_t>(kingSq), occNew);
            uint64_t rq = oppRooks | oppQueens;
            if (mask & rq) return false;
        }

        // Sliding bishop/queen (diagonal)
        {
            uint64_t mask = pieces::getBishopAttacks(static_cast<int16_t>(kingSq), occNew);
            uint64_t bq = oppBishops | oppQueens;
            if (mask & bq) return false;
        }
    }

    return true;
}

// ------------------------------------------------------------
// CHECK / CHECKMATE / STALEMATE UTILITIES
// ------------------------------------------------------------
// Returns true if square 'targetIndex' is attacked by 'byColor'
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept {
    // Use per-piece bitboards to test attacks quickly
    const uint64_t occ = this->occupancy;
    const bool byWhite = (byColor == WHITE);
    // Pawns: any pawn of byColor that attacks target?
    uint64_t pawnAttackers = pieces::PAWN_ATTACKERS_TO[byWhite][static_cast<int16_t>(targetIndex)];
    if (pawnAttackers & (byWhite ? pawns_bb[0] : pawns_bb[1])) return true;
    // Knights
    if (pieces::KNIGHT_ATTACKS[static_cast<int16_t>(targetIndex)] & (byWhite ? knights_bb[0] : knights_bb[1])) return true;
    // Kings (adjacent)
    if (pieces::KING_ATTACKS[static_cast<int16_t>(targetIndex)] & (byWhite ? kings_bb[0] : kings_bb[1])) return true;
    // Sliding: rook/queen
    {
        uint64_t mask = pieces::getRookAttacks(static_cast<int16_t>(targetIndex), occ);
        if (mask & (byWhite ? (rooks_bb[0] | queens_bb[0]) : (rooks_bb[1] | queens_bb[1]))) return true;
    }
    // Sliding: bishop/queen
    {
        uint64_t mask = pieces::getBishopAttacks(static_cast<int16_t>(targetIndex), occ);
        if (mask & (byWhite ? (bishops_bb[0] | queens_bb[0]) : (bishops_bb[1] | queens_bb[1]))) return true;
    }
    
    return false;
}

// Version that excludes a square from occupancy - useful for king moves
bool Board::isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept {
    // Exclude the specified square from occupancy when checking attacks
    const uint64_t occ = this->occupancy & ~(1ULL << excludeSquare);
    const bool byWhite = (byColor == WHITE);
    
    // Pawns: any pawn of byColor that attacks target?
    uint64_t pawnAttackers = pieces::PAWN_ATTACKERS_TO[byWhite][static_cast<int16_t>(targetIndex)];
    if (pawnAttackers & (byWhite ? pawns_bb[0] : pawns_bb[1])) return true;
    // Knights
    if (pieces::KNIGHT_ATTACKS[static_cast<int16_t>(targetIndex)] & (byWhite ? knights_bb[0] : knights_bb[1])) return true;
    // Kings (adjacent)
    if (pieces::KING_ATTACKS[static_cast<int16_t>(targetIndex)] & (byWhite ? kings_bb[0] : kings_bb[1])) return true;
    // Sliding: rook/queen (with modified occupancy)
    {
        uint64_t mask = pieces::getRookAttacks(static_cast<int16_t>(targetIndex), occ);
        if (mask & (byWhite ? (rooks_bb[0] | queens_bb[0]) : (rooks_bb[1] | queens_bb[1]))) return true;
    }
    // Sliding: bishop/queen (with modified occupancy)
    {
        uint64_t mask = pieces::getBishopAttacks(static_cast<int16_t>(targetIndex), occ);
        if (mask & (byWhite ? (bishops_bb[0] | queens_bb[0]) : (bishops_bb[1] | queens_bb[1]))) return true;
    }
    
    return false;
}

// Is the given color currently in check?
bool Board::inCheck(uint8_t color) const noexcept {
    // Find king square using king bitboards
    const uint64_t kingBB = (color == WHITE) ? kings_bb[0] : kings_bb[1];
    if (!kingBB) {
        return false; // no king found (invalid position) -> treat as not in check
    }

    const uint8_t kingIndex = __builtin_ctzll(kingBB);
    const uint8_t opp = (color == WHITE) ? BLACK : WHITE;
    return isSquareAttacked(kingIndex, opp);
}


bool Board::hasAnyLegalMove(uint8_t color) const noexcept {
    const uint64_t occ = this->occupancy;

    const bool isWhite = (color == WHITE);
    const int side = isWhite ? 0 : 1;

    // Precompute our own occupancy mask once
    const uint64_t ownOcc = pawns_bb[side] | knights_bb[side] | bishops_bb[side] |
                            rooks_bb[side] | queens_bb[side]  | kings_bb[side];

    // Helper lambda to test moves for a given piece type bitboard
    auto tryMovesFromBitboard = [&](uint64_t piecesBB, auto genMovesForSquare) -> bool {
        while (piecesBB) {
            const uint8_t from = __builtin_ctzll(piecesBB);
            piecesBB &= (piecesBB - 1);

            const uint8_t r = from >> 3;
            const uint8_t f = from & 7;

            // Generate pseudo-legal moves and clear own-occupied squares once
            uint64_t movesMask = genMovesForSquare(from, occ) & ~ownOcc;

            while (movesMask) {
                const uint8_t to = __builtin_ctzll(movesMask);
                movesMask &= (movesMask - 1);

                const uint8_t tr = to >> 3;
                const uint8_t tf = to & 7;

                // Use canMoveToBB which already checks legality (pseudo-legal + king safety)
                if (this->canMoveToBB(Coords{f, r}, Coords{tf, tr})) return true;
            }
        }
        return false;
    };

    // PAWNS: handle pushes, captures, promotions (incl. en-passant) via bitboards
    {
        uint64_t pawns = pawns_bb[side];
        // Enemy occupancy mask (for real captures); en-passant è gestito da moveBB
        const uint64_t enemyOcc = side == 0
            ? (pawns_bb[1] | knights_bb[1] | bishops_bb[1] | rooks_bb[1] | queens_bb[1] | kings_bb[1])
            : (pawns_bb[0] | knights_bb[0] | bishops_bb[0] | rooks_bb[0] | queens_bb[0] | kings_bb[0]);

        while (pawns) {
            const uint8_t from = __builtin_ctzll(pawns);
            pawns &= (pawns - 1);

            const uint8_t r = from >> 3;
            const uint8_t f = from & 7;

            const uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][from];
            const uint64_t pushes  = pieces::getPawnForwardPushes(from, isWhite, occupancy);

            // PUSHS (sempre su case vuote)
            uint64_t pushMask = pushes;
            while (pushMask) {
                const uint8_t to = __builtin_ctzll(pushMask);
                pushMask &= (pushMask - 1);

                const uint8_t tr = to >> 3;
                const uint8_t tf = to & 7;

                const bool isPromotion = isWhite ? (tr == 7) : (tr == 0);
                if (isPromotion) {
                    // For promotions, if the pawn move itself is legal (checked by canMoveToBB),
                    // then all promotion choices (Q/R/B/N) are legal since the choice of promoted
                    // piece doesn't affect the legality of the move itself.
                    if (this->canMoveToBB(Coords{f, r}, Coords{tf, tr})) return true;
                    continue;
                }

                // Use canMoveToBB for non-promotion pawn pushes
                if (this->canMoveToBB(Coords{f, r}, Coords{tf, tr})) return true;
            }

            // CAPTURES (reali + en-passant, filtrate a livello di Board::moveBB)
            uint64_t captureMask = attacks & (enemyOcc | ~occ); // include possibili EP su case "vuote"
            while (captureMask) {
                const uint8_t to = __builtin_ctzll(captureMask);
                captureMask &= (captureMask - 1);

                const uint8_t tr = to >> 3;
                const uint8_t tf = to & 7;

                const bool isPromotion = isWhite ? (tr == 7) : (tr == 0);
                if (isPromotion) {
                    // For promotions, if the pawn move itself is legal (checked by canMoveToBB),
                    // then all promotion choices (Q/R/B/N) are legal.
                    if (this->canMoveToBB(Coords{f, r}, Coords{tf, tr})) return true;
                    continue;
                }

                // Use canMoveToBB for non-promotion pawn captures
                if (this->canMoveToBB(Coords{f, r}, Coords{tf, tr})) return true;
            }
        }
    }

    // KNIGHTS
    if (tryMovesFromBitboard(knights_bb[side], [&](uint8_t sq, uint64_t /*occBB*/) {
            return pieces::KNIGHT_ATTACKS[sq];
        })) return true;

    // BISHOPS
    if (tryMovesFromBitboard(bishops_bb[side], [&](uint8_t sq, uint64_t occBB) {
            return pieces::getBishopAttacks(sq, occBB);
        })) return true;

    // ROOKS
    if (tryMovesFromBitboard(rooks_bb[side], [&](uint8_t sq, uint64_t occBB) {
            return pieces::getRookAttacks(sq, occBB);
        })) return true;

    // QUEENS
    if (tryMovesFromBitboard(queens_bb[side], [&](uint8_t sq, uint64_t occBB) {
            return pieces::getQueenAttacks(sq, occBB);
        })) return true;

    // KINGS (including castling via canMoveToBB)
    {
        uint64_t kings = kings_bb[side];
        if (kings) {
            const uint8_t from = __builtin_ctzll(kings);
            const uint8_t r = (from >> 3);
            const uint8_t f = (from & 7);

            // Pseudo-legal king moves, already excluding our own pieces
            uint64_t movesMask = pieces::KING_ATTACKS[from] & ~ownOcc;

            // Castling squares: rely on canMoveToBB for full legality (including checks)
            if (f + 2 <= 7) {
                Coords toKs{static_cast<uint8_t>(f + 2), r};
                if (this->canMoveToBB(Coords{f, r}, toKs)) {
                    movesMask |= (1ULL << toKs.toIndex());
                }
            }
            if (f >= 2) {
                Coords toQs{static_cast<uint8_t>(f - 2), r};
                if (this->canMoveToBB(Coords{f, r}, toQs)) {
                    movesMask |= (1ULL << toQs.toIndex());
                }
            }

            while (movesMask) {
                const uint8_t to = __builtin_ctzll(movesMask);
                movesMask &= (movesMask - 1);

                const uint8_t tr = (to >> 3);
                const uint8_t tf = (to & 7);

                // Use canMoveToBB for king moves (including castling already checked above)
                if (this->canMoveToBB(Coords{f, r}, Coords{tf, tr})) return true;
            }
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
        if (from.file != to.file && destBefore == EMPTY &&
            Coords::isInBounds(st.prevEnPassant[0]) &&
            toIndex == st.prevEnPassant[0].toIndex()) {

            st.wasEnPassantCapture = true;

            const bool isWhite = (movingColor == WHITE);
            const int8_t forwardDir = isWhite ? 1 : -1;
            Coords captured{to.file, static_cast<uint8_t>(to.rank - forwardDir)};
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
    if (movingType == KING && from.rank == to.rank) {
        const int8_t df = static_cast<int8_t>(to.file - from.file);
        if (df == 2 || df == -2) {
            st.wasCastling = true;

            // Compute rook indices directly without intermediate Coords
            const uint8_t rookFromFile = (df == 2) ? 7 : 0;
            const uint8_t rookToFile   = (df == 2) ? 5 : 3;
            const uint8_t rookFromIndex = (to.rank << 3) | rookFromFile;
            const uint8_t rookToIndex   = (to.rank << 3) | rookToFile;

            st.rookFromIndex = rookFromIndex;
            st.rookToIndex   = rookToIndex;

            const uint8_t rook = this->get(to.rank, rookFromFile);
            this->set(to.rank, rookToFile, static_cast<piece_id>(rook));
            this->set(to.rank, rookFromFile, EMPTY);
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
        const bool isInitialSquare = (movingColor == WHITE)
            ? (from.rank == 0 && (from.file == 0 || from.file == 7))
            : (from.rank == 7 && (from.file == 0 || from.file == 7));
        
        if (isInitialSquare) {
            if (movingColor == WHITE) {
                if (from.file == 0) {
                    castle &= ~(1u << 1); // white queenside
                    hasMoved |= (1u << 1);
                } else {
                    castle &= ~(1u << 0); // white kingside
                    hasMoved |= (1u << 2);
                }
            } else {
                if (from.file == 0) {
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
        const bool isInitialSquare = ((destBefore & MASK_COLOR) == WHITE)
            ? (to.rank == 0 && (to.file == 0 || to.file == 7))
            : (to.rank == 7 && (to.file == 0 || to.file == 7));
        
        if (isInitialSquare) {
            if ((destBefore & MASK_COLOR) == WHITE) {
                castle &= (to.file == 0) 
                    ? ~(1u << 1)  // white queenside
                    : ~(1u << 0); // white kingside
            } else {
                castle &= (to.file == 0)
                    ? ~(1u << 3)  // black queenside
                    : ~(1u << 2); // black kingside
            }
        }
    }

    // --- EN PASSANT TARGET DOPO DOPPIO PASSO ---
    if (movingType == PAWN) {
        const int8_t dr = static_cast<int8_t>(to.rank - from.rank);
        if (dr == 2 || dr == -2) {
            enPassant[0] = Coords{from.file, static_cast<uint8_t>((from.rank + to.rank) >> 1)};
        }
    }

    // --- PROMOZIONE ---
    if (movingType == PAWN) {
        if ((movingColor == WHITE && to.rank == 7) ||
            (movingColor == BLACK && to.rank == 0)) {

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
        const uint8_t capRank = capIndex >> 3;
        const uint8_t capFile = capIndex & 7;
        this->set(capRank, capFile, static_cast<piece_id>(st.capturedPiece));
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
        const uint8_t rank = rookFromIndex >> 3;
        const uint8_t rookFromFile = rookFromIndex & 7;
        const uint8_t rookToFile   = rookToIndex & 7;
        
        const uint8_t rook = this->get(rank, rookToFile);
        this->set(rank, rookFromFile, static_cast<piece_id>(rook));
        this->set(rank, rookToFile, EMPTY);
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
