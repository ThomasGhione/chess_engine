#include "board.hpp"

namespace chess {

void Board::fromFenToBoard(const std::string& fen) {
    std::array<uint32_t, 8> parsedBoard{};
    std::vector<bool> parsedCastle(4, false);
    std::array<Coords, 2> parsedEnPassant{Coords{}, Coords{}};
    uint8_t parsedHalfMove = 0;
    uint8_t parsedFullMove = 1;
    uint8_t parsedActiveColor = WHITE;

    std::istringstream fenStream(fen);
    std::string boardSection;
    std::string activeSection;
    std::string castlingSection;
    std::string enPassantSection;
    std::string halfMoveSection;
    std::string fullMoveSection;

    if (!(fenStream >> boardSection >> activeSection >> castlingSection >> enPassantSection >> halfMoveSection >> fullMoveSection)) {
        return;
    }

    std::istringstream boardStream(boardSection);
    std::string rankSegment;
    int rankIndex = 7;
    while (std::getline(boardStream, rankSegment, '/') && rankIndex >= 0) {
        int fileIndex = 0;
        for (char symbol : rankSegment) {
            if (std::isdigit(static_cast<unsigned char>(symbol))) {
                fileIndex += symbol - '0';
                if (fileIndex > 8) {
                    return;
                }
                continue;
            }

            if (fileIndex >= 8) {
                return;
            }

            const bool isWhitePiece = std::isupper(static_cast<unsigned char>(symbol));
            const char lowerSymbol = static_cast<char>(std::tolower(static_cast<unsigned char>(symbol)));

            uint8_t pieceType = EMPTY;
            switch (lowerSymbol) {
                case 'p': pieceType = PAWN; break;
                case 'n': pieceType = KNIGHT; break;
                case 'b': pieceType = BISHOP; break;
                case 'r': pieceType = ROOK; break;
                case 'q': pieceType = QUEEN; break;
                case 'k': pieceType = KING; break;
                default: return;
            }

            uint8_t encodedPiece = pieceType;
            if (!isWhitePiece) {
                encodedPiece |= BLACK;
            }

            parsedBoard.at(rankIndex) |= static_cast<uint32_t>(encodedPiece) << (fileIndex * 4);
            ++fileIndex;
        }

        if (fileIndex != 8) {
            return;
        }

        --rankIndex;
    }

    if (rankIndex != -1) {
        return;
    }

    if (!activeSection.empty() && (activeSection[0] == 'b' || activeSection[0] == 'B')) {
        parsedActiveColor = BLACK;
    }

    if (castlingSection != "-") {
        for (char castleChar : castlingSection) {
            switch (castleChar) {
                case 'K': parsedCastle[0] = true; break;
                case 'Q': parsedCastle[1] = true; break;
                case 'k': parsedCastle[2] = true; break;
                case 'q': parsedCastle[3] = true; break;
                default: break;
            }
        }
    }

    if (enPassantSection.size() == 2 && enPassantSection != "-") {
        char fileChar = enPassantSection[0];
        char rankChar = enPassantSection[1];
        if (fileChar >= 'a' && fileChar <= 'h' && rankChar >= '1' && rankChar <= '8') {
            uint8_t file = static_cast<uint8_t>(fileChar - 'a');
            uint8_t rank = static_cast<uint8_t>(rankChar - '1');
            parsedEnPassant[0] = Coords(file, rank);
        }
    }

    try {
        int halfMove = std::stoi(halfMoveSection);
        halfMove = std::clamp(halfMove, 0, 255);
        parsedHalfMove = static_cast<uint8_t>(halfMove);
    } catch (...) {
        parsedHalfMove = 0;
    }

    try {
        int fullMove = std::stoi(fullMoveSection);
        fullMove = std::clamp(fullMove, 1, 255);
        parsedFullMove = static_cast<uint8_t>(fullMove);
    } catch (...) {
        parsedFullMove = 1;
    }

    chessboard = parsedBoard;
    castle = parsedCastle;
    enPassant = parsedEnPassant;
    halfMoveClock = parsedHalfMove;
    fullMoveClock = parsedFullMove;
    activeColor = parsedActiveColor;

    this->updateOccupancyBB();
}


std::string Board::fromBoardToFen() const {
    std::string fen;
    fen.reserve(90);

    for (int rank = 7; rank >= 0; --rank) {
        int emptySquaresCounter = 0;

        for (int file = 0; file < 8; ++file) {
            const uint8_t rawPiece = static_cast<uint8_t>((chessboard.at(rank) >> (file * 4)) & MASK_PIECE);
            const uint8_t pieceType = rawPiece & MASK_PIECE_TYPE;

            if (pieceType == EMPTY) {
                ++emptySquaresCounter;
                continue;
            }

            if (emptySquaresCounter > 0) {
                fen += std::to_string(emptySquaresCounter);
                emptySquaresCounter = 0;
            }

            char symbol = '?';
            switch (pieceType) {
                case PAWN:   symbol = 'p'; break;
                case KNIGHT: symbol = 'n'; break;
                case BISHOP: symbol = 'b'; break;
                case ROOK:   symbol = 'r'; break;
                case QUEEN:  symbol = 'q'; break;
                case KING:   symbol = 'k'; break;
                default:     symbol = '?'; break;
            }

            const bool isWhitePiece = (rawPiece & MASK_COLOR) == WHITE;
            if (isWhitePiece) {
                symbol = static_cast<char>(std::toupper(static_cast<unsigned char>(symbol)));
            }

            fen += symbol;
        }

        if (emptySquaresCounter > 0) {
            fen += std::to_string(emptySquaresCounter);
        }

        if (rank > 0) {
            fen += '/';
        }
    }

    fen += ' ';
    fen += (activeColor == WHITE) ? 'w' : 'b';

    fen += ' ';
    std::string castlingStr;
    if (castle[0]) { castlingStr += 'K'; }
    if (castle[1]) { castlingStr += 'Q'; }
    if (castle[2]) { castlingStr += 'k'; }
    if (castle[3]) { castlingStr += 'q'; }
    fen += castlingStr.empty() ? "-" : castlingStr;

    fen += ' ';
    const Coords* epSquare = nullptr;
    for (const auto& candidate : enPassant) {
        if (Coords::isInBounds(candidate)) {
            epSquare = &candidate;
            break;
        }
    }

    if (epSquare == nullptr) {
        fen += '-';
    } else {
        fen += static_cast<char>('a' + epSquare->file);
        fen += static_cast<char>('1' + epSquare->rank);
    }

    fen += ' ';
    fen += std::to_string(halfMoveClock);

    fen += ' ';
    fen += std::to_string(fullMoveClock);

    return fen;
}



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
    auto disableWhiteKingside = [&]{ if (castle.size()>=1) castle[0] = false; };
    auto disableWhiteQueenside = [&]{ if (castle.size()>=2) castle[1] = false; };
    auto disableBlackKingside = [&]{ if (castle.size()>=3) castle[2] = false; };
    auto disableBlackQueenside = [&]{ if (castle.size()>=4) castle[3] = false; };

    if (movingType == KING) {
        if (movingColor == WHITE) { 
            disableWhiteKingside(); 
            disableWhiteQueenside(); 
            if (hasMoved.size()>=1) hasMoved[0] = true;
        }
        else { 
            disableBlackKingside(); 
            disableBlackQueenside(); 
            if (hasMoved.size()>=4) hasMoved[3] = true;
         }
    }
    if (movingType == ROOK) {
        if (movingColor == WHITE) {
            if (from.rank == 0 && from.file == 0) { 
                disableWhiteQueenside();
                if (hasMoved.size()>=2) hasMoved[1] = true;
            }
            if (from.rank == 0 && from.file == 7) { 
                disableWhiteKingside();
                if (hasMoved.size()>=3) hasMoved[2] = true;
            }
        } else {
            if (from.rank == 7 && from.file == 0) { 
                disableBlackQueenside();
                if (hasMoved.size()>=5) hasMoved[4] = true;
            }
            if (from.rank == 7 && from.file == 7) { 
                disableBlackKingside();
                if (hasMoved.size()>=6) hasMoved[5] = true;
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
            uint8_t midRank = static_cast<uint8_t>((from.rank + to.rank) / 2);
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
        // Locate king and count attackers
        for (uint8_t idx = 0; idx < 64; ++idx) {
            uint8_t pc = this->get(idx / 8, idx % 8);
            if (((pc & MASK_PIECE_TYPE) == KING) && ((pc & MASK_COLOR) == movingColor)) {
                kingIndex = idx;
                break;
            }
        }
        // Count attackers on king square
        if (kingIndex < 64) {
            // brute force count
            for (uint8_t idx = 0; idx < 64; ++idx) {
                uint8_t pc = this->get(idx / 8, idx % 8);
                if ((pc & MASK_PIECE_TYPE) == EMPTY || (pc & MASK_COLOR) != oppColor) continue;
                // Attack test: reuse isSquareAttacked logic by temporarily targeting piece square? Instead generate attacks of pc and see if kingIndex reachable.
                // Simplify: call isSquareAttacked(kingIndex, oppColor) only once -> if true attackerCount>=1.
                // For double-check detection we approximate by scanning every potential removal: more expensive; keep simple: attackerCount= isSquareAttacked?1:0.
            }
            // We only have boolean now; refine to proper count by enumerating piece types.
            // Build occupancy copy and test each enemy piece separately.
            for (uint8_t idx = 0; idx < 64; ++idx) {
                uint8_t pc = this->get(idx / 8, idx % 8);
                if ((pc & MASK_PIECE_TYPE) == EMPTY || (pc & MASK_COLOR) != oppColor) continue;
                uint8_t pt = pc & MASK_PIECE_TYPE;
                uint64_t atkMask = 0ULL;
                switch (pt) {
                    case PAWN: {
                        bool isWhiteEnemy = (pc & MASK_COLOR) == WHITE;
                        atkMask = pieces::getPawnAttacks(idx, isWhiteEnemy);
                        break; 
                    }
                    case KNIGHT: atkMask = pieces::getKnightAttacks(idx); break;
                    case BISHOP: atkMask = pieces::getBishopAttacks(idx, occ); break;
                    case ROOK:   atkMask = pieces::getRookAttacks(idx, occ); break;
                    case QUEEN:  atkMask = pieces::getQueenAttacks(idx, occ); break;
                    case KING:   atkMask = pieces::getKingAttacks(idx); break;
                    default: break;
                }
                if (atkMask & (1ULL << kingIndex)) attackerCount++;
            }
        }
    }

    // Double check: only king moves allowed
    if (inChk && attackerCount >= 2 && fromType != KING) return false;
    

    switch (fromType) { // piece type only
        case PAWN: {
            const bool isWhite = (this->getColor(from) == WHITE);
            const uint64_t attacks = pieces::getPawnAttacks(fromIndex, isWhite);
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
            // Always ensure move doesn't leave king in check (handles pins too)
            Board copy = *this;
            copy.updateChessboard(from, to);
            if (isEnPassant) {
                int8_t dir = isWhite ? 1 : -1;
                Coords captured{to.file, static_cast<uint8_t>(to.rank - dir)};
                copy.set(captured, EMPTY);
            }
            copy.updateOccupancyBB();
            if (copy.inCheck(movingColor)) return false;
            return true;
        }
        case KNIGHT:
            bitMap = pieces::getKnightAttacks(fromIndex); break;
        case BISHOP:
            bitMap = pieces::getBishopAttacks(fromIndex, occ); break;
        case ROOK:
            bitMap = pieces::getRookAttacks(fromIndex, occ); break;
        case QUEEN:
            bitMap = pieces::getQueenAttacks(fromIndex, occ); break;
        case KING: {
            bitMap = pieces::getKingAttacks(fromIndex);
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
                    bool rights = isWhite ? (castle.size()>=1 && castle[0]) : (castle.size()>=3 && castle[2]);
                    bool emptyBetween = (this->get(r, static_cast<uint8_t>(kf + 1)) == EMPTY) && (this->get(r, static_cast<uint8_t>(kf + 2)) == EMPTY);
                    {
                        uint8_t rookPiece = this->get(r, static_cast<uint8_t>(kf + 3));
                        bool rookOk = ((rookPiece & MASK_PIECE_TYPE) == ROOK) && ((rookPiece & MASK_COLOR) == (isWhite ? WHITE : BLACK));
                        if (!rookOk) {
                            // fallthrough
                        } else {
                            uint8_t eIdx = static_cast<uint8_t>(r * 8 + kf);
                            uint8_t fIdx = static_cast<uint8_t>(r * 8 + (kf + 1));
                            uint8_t gIdx = static_cast<uint8_t>(r * 8 + (kf + 2));
                            uint8_t opp = isWhite ? BLACK : WHITE;
                            bool safe = !isSquareAttacked(eIdx, opp) && !isSquareAttacked(fIdx, opp) && !isSquareAttacked(gIdx, opp);
                            if (rights && emptyBetween && rookOk && safe) return true;
                        }
                    }
                } else if (df == -2) { // queenside
                    bool rights = isWhite ? (castle.size()>=2 && castle[1]) : (castle.size()>=4 && castle[3]);
                    bool emptyBetween = (this->get(r, static_cast<uint8_t>(kf - 1)) == EMPTY) && (this->get(r, static_cast<uint8_t>(kf - 2)) == EMPTY) && (this->get(r, static_cast<uint8_t>(kf - 3)) == EMPTY);
                    {
                        uint8_t rookPiece = this->get(r, static_cast<uint8_t>(kf - 4));
                        bool rookOk = ((rookPiece & MASK_PIECE_TYPE) == ROOK) && ((rookPiece & MASK_COLOR) == (isWhite ? WHITE : BLACK));
                        uint8_t eIdx = static_cast<uint8_t>(r * 8 + kf);
                        uint8_t dIdx = static_cast<uint8_t>(r * 8 + (kf - 1));
                        uint8_t cIdx = static_cast<uint8_t>(r * 8 + (kf - 2));
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

    // King move already prevented into attacked square. For any non-king move, ensure king safety (pins and check resolution)
    if (fromType != KING) {
        Board copy = *this;
        copy.updateChessboard(from, to);
        copy.updateOccupancyBB();
        if (copy.inCheck(movingColor)) return false;
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
    uint64_t pawnAttackers = pieces::getPawnAttackersTo(static_cast<int16_t>(targetIndex), byWhite);
    if (pawnAttackers & (byWhite ? pawns_bb[0] : pawns_bb[1])) return true;
    // Knights
    if (pieces::getKnightAttacks(static_cast<int16_t>(targetIndex)) & (byWhite ? knights_bb[0] : knights_bb[1])) return true;
    // Kings (adjacent)
    if (pieces::getKingAttacks(static_cast<int16_t>(targetIndex)) & (byWhite ? kings_bb[0] : kings_bb[1])) return true;
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

// Is the given color currently in check?
bool Board::inCheck(uint8_t color) const noexcept {
    // Find king square
    for (uint8_t idx = 0; idx < 64; ++idx) {
        const uint8_t r = static_cast<uint8_t>(idx / 8);
        const uint8_t f = static_cast<uint8_t>(idx % 8);
        const uint8_t p = this->get(r, f);
        if (((p & MASK_PIECE_TYPE) == KING) && ((p & MASK_COLOR) == color)) {
            const uint8_t opp = (color == WHITE) ? BLACK : WHITE;
            return isSquareAttacked(idx, opp);
        }
    }
    return false; // no king found (invalid position) -> treat as not in check
}


bool Board::hasAnyLegalMove(uint8_t color) const noexcept {
    const uint64_t occ = this->occupancy;

    for (uint8_t from = 0; from < 64; ++from) {
        const uint8_t r = static_cast<uint8_t>(from / 8);
        const uint8_t f = static_cast<uint8_t>(from % 8);
        const uint8_t p = this->get(r, f);
        if ((p & MASK_PIECE_TYPE) == EMPTY) continue;
        if ((p & MASK_COLOR) != color) continue;

        const uint8_t pt = (p & MASK_PIECE_TYPE);
        uint64_t movesMask = 0ULL;

        switch (pt) {
            case PAWN: {
                const bool isWhite = (color == WHITE);
                const uint64_t attacks = pieces::getPawnAttacks(from, isWhite);
                const uint64_t pushes  = pieces::getPawnForwardPushes(from, isWhite, occ);
                movesMask = attacks | pushes;
                break;
            }
            case KNIGHT:
                movesMask = pieces::getKnightAttacks(from);
                break;
            case BISHOP:
                movesMask = pieces::getBishopAttacks(from, occ);
                break;
            case ROOK:
                movesMask = pieces::getRookAttacks(from, occ);
                break;
            case QUEEN:
                movesMask = pieces::getQueenAttacks(from, occ);
                break;
            case KING:
                movesMask = pieces::getKingAttacks(from);
                // Also consider castling squares if legal per canMoveToBB
                {
                    const uint8_t rK = r;
                    const uint8_t fK = f;
                    // kingside
                    if (fK + 2 <= 7) {
                        Coords toKs{static_cast<uint8_t>(fK + 2), rK};
                        if (this->canMoveToBB(Coords{fK, rK}, toKs)) {
                            movesMask |= (1ULL << toKs.toIndex());
                        }
                    }
                    // queenside
                    if (fK >= 2) {
                        Coords toQs{static_cast<uint8_t>(fK - 2), rK};
                        if (this->canMoveToBB(Coords{fK, rK}, toQs)) {
                            movesMask |= (1ULL << toQs.toIndex());
                        }
                    }
                }
                break;
            default:
                continue;
        }

        for (uint8_t to = 0; to < 64; ++to) {
            if ((movesMask & (1ULL << to)) == 0ULL) continue;
            const uint8_t tr = static_cast<uint8_t>(to / 8);
            const uint8_t tf = static_cast<uint8_t>(to % 8);
            const uint8_t dst = this->get(tr, tf);
            // Skip own-occupied destination
            if ((dst != EMPTY) && ((dst & MASK_COLOR) == color)) continue;

            // Try the move on a copy and verify king safety
            if (pt == PAWN) {
                const bool isPromotion = (color == WHITE ? (tr == 7) : (tr == 0));
                if (isPromotion) {
                    const char promos[4] = {'q','r','b','n'};
                    for (char ch : promos) {
                        Board copy = *this;
                        if (!copy.moveBB(Coords{f, r}, Coords{tf, tr}, ch)) continue;
                        if (!copy.inCheck(color)) return true;
                    }
                    continue; // no promotion resulted in legal move
                }
            }

            Board copy = *this;
            if (!copy.moveBB(Coords{f, r}, Coords{tf, tr})) continue;
            if (!copy.inCheck(color)) return true;
        }
    }
    return false;
}

}; // namespace chess
