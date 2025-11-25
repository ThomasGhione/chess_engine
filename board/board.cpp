#include "board.hpp"

namespace chess {
// Converts a single FEN character
uint8_t Board::charToPiece(char symbol) {
    const bool isWhite = std::isupper(static_cast<unsigned char>(symbol));
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(symbol)));

    uint8_t pieceType = EMPTY;
    switch (lower) {
        case 'p': pieceType = PAWN;   break;
        case 'n': pieceType = KNIGHT; break;
        case 'b': pieceType = BISHOP; break;
        case 'r': pieceType = ROOK;   break;
        case 'q': pieceType = QUEEN;  break;
        case 'k': pieceType = KING;   break;
        default:  return EMPTY;
    }
    return isWhite ? pieceType : static_cast<uint8_t>(pieceType | BLACK);
}
// Parses the board section of the FEN 
bool Board::parseBoardSection(const std::string& boardSection, std::array<uint32_t, 8>& parsedBoard) {
    std::istringstream boardStream(boardSection);
    std::string rankSegment;
    int rankIndex = 7;

    while (std::getline(boardStream, rankSegment, '/') && rankIndex >= 0) {
        int fileIndex = 0;
        for (char symbol : rankSegment) {
            if (std::isdigit(static_cast<unsigned char>(symbol))) {
                fileIndex += symbol - '0';
            if (fileIndex > 8) 
                return false;
            } else {
                if (fileIndex >= 8) 
                    return false;
                uint8_t piece = charToPiece(symbol);
                if (piece == EMPTY) return false;

                parsedBoard.at(rankIndex) |= static_cast<uint32_t>(piece) << (fileIndex * 4);
                ++fileIndex;
            }
        }
        if (fileIndex != 8) return false;
        --rankIndex;
    }
    return rankIndex == -1;
}
// Reads the castling rights section of the FEN
uint8_t Board::parseActiveColor(const std::string& activeSection) {
    if (!activeSection.empty() && (activeSection[0] == 'b' || activeSection[0] == 'B')) {
        return BLACK;
    }
    return WHITE;
}
std::vector<bool> Board::parseCastling(const std::string& castlingSection) {
    std::vector<bool> castle(4, false);
    if (castlingSection == "-") return castle;

    for (char c : castlingSection) {
        switch (c) {
            case 'K': castle[0] = true; break;
            case 'Q': castle[1] = true; break;
            case 'k': castle[2] = true; break;
            case 'q': castle[3] = true; break;
            default: break; // carattere ignoto: ignoriamo
        }
    }
    return castle;
}
// Reads the en passant target square section of the FEN
Coords Board::parseEnPassant(const std::string& enPassantSection) {
    if (enPassantSection.size() != 2 || enPassantSection == "-") {
        return Coords{};
    }
    char fileChar = enPassantSection[0];
    char rankChar = enPassantSection[1];

    if (fileChar < 'a' || fileChar > 'h' || rankChar < '1' || rankChar > '8') {
        return Coords{};
    }
    uint8_t file = static_cast<uint8_t>(fileChar - 'a');
    uint8_t rank = static_cast<uint8_t>(rankChar - '1');
    return Coords(file, rank);
}
// Safely converts a string to an integer with error handling.
uint8_t Board::safeParseInt(const std::string& section, int min, int max, int defaultValue) {
    try {
        int value = std::stoi(section);
        value = std::clamp(value, min, max);
        return static_cast<uint8_t>(value);
    } catch (...) {
        return static_cast<uint8_t>(defaultValue);
    }
}

void Board::fromFenToBoard(const std::string& fen) {
    std::istringstream fenStream(fen);
    std::string boardSection, activeSection, castlingSection, enPassantSection, halfMoveSection, fullMoveSection;

    if (!(fenStream >> boardSection >> activeSection >> castlingSection >> enPassantSection >> halfMoveSection >> fullMoveSection)) {
        return;
    }

    std::array<uint32_t, 8> parsedBoard{};
    if (!parseBoardSection(boardSection, parsedBoard)) {
        return;
    }

    this->chessboard = parsedBoard;
    this->activeColor = parseActiveColor(activeSection);
    this->castle = parseCastling(castlingSection);

    
    this->enPassant = {parseEnPassant(enPassantSection), Coords{}};

    this->halfMoveClock = safeParseInt(halfMoveSection, 0, 255, 0);
    this->fullMoveClock = safeParseInt(fullMoveSection, 1, 255, 1);

    this->updateOccupancyBB();
}

// Converts the internal board representation into the FEN piece placement string
std::string Board::boardToFenPieces() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int emptySquares = 0;
        for (int file = 0; file < 8; ++file) {
            const uint8_t rawPiece = static_cast<uint8_t>((chessboard.at(rank) >> (file * 4)) & MASK_PIECE);
            const uint8_t pieceType = rawPiece & MASK_PIECE_TYPE;

            if (pieceType == EMPTY) {
                ++emptySquares;
                continue;
            }

            if (emptySquares > 0) {
                fen.append(std::to_string(emptySquares));
                emptySquares = 0;
            }

            char symbol = pieceTypeToChar(pieceType);
            if ((rawPiece & MASK_COLOR) == WHITE) {
                symbol = static_cast<char>(std::toupper(static_cast<unsigned char>(symbol)));
            }
            fen.push_back(symbol);
        }
        if (emptySquares > 0) fen.append(std::to_string(emptySquares));
        if (rank > 0) fen.push_back('/');
    }
    return fen;
}

// Maps internal piece type codes to FEN characters
char Board::pieceTypeToChar(uint8_t pieceType) const {
    switch (pieceType) {
        case PAWN:   return 'p';
        case KNIGHT: return 'n';
        case BISHOP: return 'b';
        case ROOK:   return 'r';
        case QUEEN:  return 'q';
        case KING:   return 'k';
        default:     return '?';
    }
}

// Converts castling rights into the FEN castling string
std::string Board::castlingToFen() const {
    std::string castlingStr;
    if (castle[0]) castlingStr.push_back('K');
    if (castle[1]) castlingStr.push_back('Q');
    if (castle[2]) castlingStr.push_back('k');
    if (castle[3]) castlingStr.push_back('q');
    return castlingStr.empty() ? "-" : castlingStr;
}
// Converts en passant target square into FEN notation
std::string Board::enPassantToFen() const {
    for (const auto& candidate : enPassant) {
        if (Coords::isInBounds(candidate)) {
            std::string ep;
            ep.push_back(static_cast<char>('a' + candidate.file));
            ep.push_back(static_cast<char>('1' + candidate.rank));
            return ep;
        }
    }
    return "-";
}

std::string Board::fromBoardToFen() const {
    std::string fen;
    fen.reserve(90);

    fen.append(boardToFenPieces());
    fen.push_back(' ');
    fen.push_back((activeColor == WHITE) ? 'w' : 'b');
    fen.push_back(' ');
    fen.append(castlingToFen());
    fen.push_back(' ');
    fen.append(enPassantToFen());
    fen.push_back(' ');
    fen.append(std::to_string(halfMoveClock));
    fen.push_back(' ');
    fen.append(std::to_string(fullMoveClock));

    return fen;
}




bool Board::moveBB(const Coords& from, const Coords& to) noexcept {   
    if (!canMoveToBB(from, to)) return false;

    const uint8_t moving      = this->get(from);
    const uint8_t movingType  = moving & this->MASK_PIECE_TYPE;
    const uint8_t movingColor = moving & this->MASK_COLOR;
    const uint8_t destBefore  = this->get(to);

    const uint8_t fromIndex = from.toIndex();
    const uint8_t toIndex   = to.toIndex();

    // Clear en passant by default; may set again after a double push
    Coords prevEp = enPassant[0];
    enPassant[0] = Coords{};
    enPassant[1] = Coords{};

    // Handle en passant capture: pawn moves diagonally into empty ep square
    if (movingType == PAWN) {
        if (from.file != to.file && destBefore == EMPTY && Coords::isInBounds(prevEp) && (toIndex == prevEp.toIndex())) {
            const bool isWhite = (movingColor == WHITE);
            int8_t forwardDir = isWhite ? 1 : -1;
            Coords captured{to.file, static_cast<uint8_t>(to.rank - forwardDir)};
            const uint8_t capturedPiece = this->get(captured);

            // Remove captured pawn from board, occupancy and bitboards
            this->set(captured, EMPTY);
            const uint8_t capIndex = captured.toIndex();
            this->occupancy &= ~(1ULL << capIndex);
            this->removePieceFromBitboards(capturedPiece, capIndex);
        }
    }

    // Handle capture on destination square (normal captures)
    if (destBefore != EMPTY) {
        this->removePieceFromBitboards(destBefore, toIndex);
        // occupancy bit for 'to' will be re-set below via fastUpdateOccupancyBB
    }

    // Move the piece in chessboard representation
    this->updateChessboard(from, to);

    // Update occupancy bitboard for from/to squares
    this->fastUpdateOccupancyBB(fromIndex, toIndex);

    // Update per-piece bitboards for the moving piece
    this->removePieceFromBitboards(moving, fromIndex);
    this->addPieceToBitboards(moving, toIndex);

    // Castling rook move if king moved two squares on same rank
    if (movingType == KING && from.rank == to.rank) {
        int df = static_cast<int>(to.file) - static_cast<int>(from.file);
        if (df == 2) {
            // kingside: rook h -> f
            Coords rookFrom{7, to.rank};
            Coords rookTo{5, to.rank};
            const uint8_t rook = this->get(rookFrom);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                const uint8_t rookFromIndex = rookFrom.toIndex();
                const uint8_t rookToIndex   = rookTo.toIndex();

                this->set(rookTo, static_cast<piece_id>(rook));
                this->set(rookFrom, EMPTY);

                // Update occupancy and rook bitboards incrementally
                this->fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
                this->removePieceFromBitboards(rook, rookFromIndex);
                this->addPieceToBitboards(rook, rookToIndex);
            }
        } else if (df == -2) {
            // queenside: rook a -> d
            Coords rookFrom{0, to.rank};
            Coords rookTo{3, to.rank};
            const uint8_t rook = this->get(rookFrom);
            if ((rook & MASK_PIECE_TYPE) == ROOK) {
                const uint8_t rookFromIndex = rookFrom.toIndex();
                const uint8_t rookToIndex   = rookTo.toIndex();

                this->set(rookTo, static_cast<piece_id>(rook));
                this->set(rookFrom, EMPTY);

                this->fastUpdateOccupancyBB(rookFromIndex, rookToIndex);
                this->removePieceFromBitboards(rook, rookFromIndex);
                this->addPieceToBitboards(rook, rookToIndex);
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
        // Use the king bitboard to find king index quickly
        const uint64_t kingBB = (movingColor == WHITE) ? kings_bb[0] : kings_bb[1];
        if (kingBB) {
            kingIndex = static_cast<uint8_t>(__builtin_ctzll(kingBB));

            const bool oppIsWhite = (oppColor == WHITE);

            // Pawns attacking king square
            {
                uint64_t pawnsAtt = pieces::getPawnAttackersTo(static_cast<int16_t>(kingIndex), oppIsWhite) &
                                    (oppIsWhite ? pawns_bb[0] : pawns_bb[1]);
                attackerCount += static_cast<uint8_t>(__builtin_popcountll(pawnsAtt));
            }

            // Knights
            {
                uint64_t knightsAtt = pieces::getKnightAttacks(static_cast<int16_t>(kingIndex)) &
                                      (oppIsWhite ? knights_bb[0] : knights_bb[1]);
                attackerCount += static_cast<uint8_t>(__builtin_popcountll(knightsAtt));
            }

            // Kings (adjacent)
            {
                uint64_t kingsAtt = pieces::getKingAttacks(static_cast<int16_t>(kingIndex)) &
                                    (oppIsWhite ? kings_bb[0] : kings_bb[1]);
                attackerCount += static_cast<uint8_t>(__builtin_popcountll(kingsAtt));
            }

            // Sliding rook/queen (orthogonal)
            {
                uint64_t rookMask = pieces::getRookAttacks(static_cast<int16_t>(kingIndex), occ);
                uint64_t rq = oppIsWhite ? (rooks_bb[0] | queens_bb[0]) : (rooks_bb[1] | queens_bb[1]);
                uint64_t rookAtt = rookMask & rq;
                attackerCount += static_cast<uint8_t>(__builtin_popcountll(rookAtt));
            }

            // Sliding bishop/queen (diagonal)
            {
                uint64_t bishopMask = pieces::getBishopAttacks(static_cast<int16_t>(kingIndex), occ);
                uint64_t bq = oppIsWhite ? (bishops_bb[0] | queens_bb[0]) : (bishops_bb[1] | queens_bb[1]);
                uint64_t bishopAtt = bishopMask & bq;
                attackerCount += static_cast<uint8_t>(__builtin_popcountll(bishopAtt));
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
    // Find king square using king bitboards
    const uint64_t kingBB = (color == WHITE) ? kings_bb[0] : kings_bb[1];
    if (!kingBB) {
        return false; // no king found (invalid position) -> treat as not in check
    }

    const uint8_t kingIndex = static_cast<uint8_t>(__builtin_ctzll(kingBB));
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
            const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(piecesBB));
            piecesBB &= (piecesBB - 1);

            const uint8_t r = static_cast<uint8_t>(from >> 3);
            const uint8_t f = static_cast<uint8_t>(from & 7);

            // Generate pseudo-legal moves and clear own-occupied squares once
            uint64_t movesMask = genMovesForSquare(from, occ) & ~ownOcc;

            while (movesMask) {
                const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(movesMask));
                movesMask &= (movesMask - 1);

                const uint8_t tr = static_cast<uint8_t>(to >> 3);
                const uint8_t tf = static_cast<uint8_t>(to & 7);

                Board copy = *this;
                if (!copy.moveBB(Coords{f, r}, Coords{tf, tr})) continue;
                if (!copy.inCheck(color)) return true;
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
            const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(pawns));
            pawns &= (pawns - 1);

            const uint8_t r = static_cast<uint8_t>(from >> 3);
            const uint8_t f = static_cast<uint8_t>(from & 7);

            const uint64_t attacks = pieces::getPawnAttacks(from, isWhite);
            const uint64_t pushes  = pieces::getPawnForwardPushes(from, isWhite, occ);

            // PUSHS (sempre su case vuote)
            uint64_t pushMask = pushes;
            while (pushMask) {
                const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(pushMask));
                pushMask &= (pushMask - 1);

                const uint8_t tr = static_cast<uint8_t>(to >> 3);
                const uint8_t tf = static_cast<uint8_t>(to & 7);

                const bool isPromotion = isWhite ? (tr == 7) : (tr == 0);
                if (isPromotion) {
                    const char promos[4] = {'q','r','b','n'};
                    for (char ch : promos) {
                        Board copy = *this;
                        if (!copy.moveBB(Coords{f, r}, Coords{tf, tr}, ch)) continue;
                        if (!copy.inCheck(color)) return true;
                    }
                    continue;
                }

                Board copy = *this;
                if (!copy.moveBB(Coords{f, r}, Coords{tf, tr})) continue;
                if (!copy.inCheck(color)) return true;
            }

            // CAPTURES (reali + en-passant, filtrate a livello di Board::moveBB)
            uint64_t captureMask = attacks & (enemyOcc | ~occ); // include possibili EP su case "vuote"
            while (captureMask) {
                const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(captureMask));
                captureMask &= (captureMask - 1);

                const uint8_t tr = static_cast<uint8_t>(to >> 3);
                const uint8_t tf = static_cast<uint8_t>(to & 7);

                const bool isPromotion = isWhite ? (tr == 7) : (tr == 0);
                if (isPromotion) {
                    const char promos[4] = {'q','r','b','n'};
                    for (char ch : promos) {
                        Board copy = *this;
                        if (!copy.moveBB(Coords{f, r}, Coords{tf, tr}, ch)) continue;
                        if (!copy.inCheck(color)) return true;
                    }
                    continue;
                }

                Board copy = *this;
                if (!copy.moveBB(Coords{f, r}, Coords{tf, tr})) continue;
                if (!copy.inCheck(color)) return true;
            }
        }
    }

    // KNIGHTS
    if (tryMovesFromBitboard(knights_bb[side], [&](uint8_t sq, uint64_t occBB) {
            return pieces::getKnightAttacks(sq);
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
            const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(kings));
            const uint8_t r = static_cast<uint8_t>(from >> 3);
            const uint8_t f = static_cast<uint8_t>(from & 7);

            // Pseudo-legal king moves, already excluding our own pieces
            uint64_t movesMask = pieces::getKingAttacks(from) & ~ownOcc;

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
                const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(movesMask));
                movesMask &= (movesMask - 1);

                const uint8_t tr = static_cast<uint8_t>(to >> 3);
                const uint8_t tf = static_cast<uint8_t>(to & 7);

                Board copy = *this;
                if (!copy.moveBB(Coords{f, r}, Coords{tf, tr})) continue;
                if (!copy.inCheck(color)) return true;
            }
        }
    }

    return false;
}

}; // namespace chess
