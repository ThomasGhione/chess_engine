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
