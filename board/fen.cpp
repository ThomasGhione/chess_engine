#include "board.hpp"
#include <charconv>

namespace chess {

// Converts a single FEN character using compile-time lookup table
uint8_t Board::charToPiece(char symbol) {
    return CHAR_TO_PIECE_TYPE[static_cast<uint8_t>(symbol)];
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
    // Legacy helper kept for compatibility but no longer used for internal storage.
    std::vector<bool> castleVec(4, false);
    if (castlingSection == "-") return castleVec;

    for (char c : castlingSection) {
        switch (c) {
            case 'K': castleVec[0] = true; break;
            case 'Q': castleVec[1] = true; break;
            case 'k': castleVec[2] = true; break;
            case 'q': castleVec[3] = true; break;
            default: break; // carattere ignoto: ignoriamo
        }
    }
    return castleVec;
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
    // Board convention: rank 0 = row 8 (top), rank 7 = row 1 (bottom)
    // FEN uses '1'-'8' where '8' is the top row -> we need to invert
    uint8_t rank = static_cast<uint8_t>('8' - rankChar);
    return Coords(file, rank);
}
// Safely converts a string to an integer with error handling.
uint8_t Board::safeParseInt(const std::string& section, int min, int max, int defaultValue) {
    if (section.empty()) {
        return static_cast<uint8_t>(defaultValue);
    }

    int value = 0;
    const char* first = section.data();
    const char* last = first + section.size();
    const auto result = std::from_chars(first, last, value);

    // Fail if parsing failed or not all characters were consumed
    if (result.ec != std::errc{} || result.ptr != last) {
        return static_cast<uint8_t>(defaultValue);
    }

    value = std::clamp(value, min, max);
    return static_cast<uint8_t>(value);
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

    // Initialize castling bitmask (KQkq) from FEN
    castle = 0x00;
    if (castlingSection != "-") {
        for (char c : castlingSection) {
            switch (c) {
                case 'K': castle |= (1u << 0); break; // white king side
                case 'Q': castle |= (1u << 1); break; // white queen side
                case 'k': castle |= (1u << 2); break; // black king side
                case 'q': castle |= (1u << 3); break; // black queen side
                default: break;
            }
        }
    }

    
    this->enPassant = parseEnPassant(enPassantSection);

    this->halfMoveClock = safeParseInt(halfMoveSection, 0, 255, 0);
    this->fullMoveClock = safeParseInt(fullMoveSection, 1, 255, 1);

    this->updateOccupancyBB();
    this->rebuildRepetitionHistory();
}

// Converts the internal board representation into the FEN piece placement string
std::string Board::boardToFenPieces() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int emptySquares = 0;
        for (int file = 0; file < 8; ++file) {
            const uint8_t rawPiece = static_cast<uint8_t>((chessboard.at(rank) >> (file * 4)) & MASK_PIECE);
            const uint8_t pieceType = rawPiece & MASK_PIECE_TYPE;
            const uint8_t pieceColor = rawPiece & MASK_COLOR;

            if (pieceType == EMPTY) {
                ++emptySquares;
                continue;
            }

            if (emptySquares > 0) {
                fen.append(std::to_string(emptySquares));
                emptySquares = 0;
            }

            char symbol = pieceTypeToChar(pieceType);
            if (pieceColor == BLACK) {
                symbol = static_cast<char>(std::tolower(static_cast<unsigned char>(symbol)));
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
    return PIECE_TYPE_TO_CHAR[pieceType & MASK_PIECE_TYPE];
}

// Converts castling rights into the FEN castling string
std::string Board::castlingToFen() const {
    std::string castlingStr;
    if (castle & (1u << 0)) castlingStr.push_back('K');
    if (castle & (1u << 1)) castlingStr.push_back('Q');
    if (castle & (1u << 2)) castlingStr.push_back('k');
    if (castle & (1u << 3)) castlingStr.push_back('q');
    return castlingStr.empty() ? "-" : castlingStr;
}
// Converts en passant target square into FEN notation
std::string Board::enPassantToFen() const {
    if (Coords::isInBounds(enPassant)) {
        std::string ep;
        // file: 0-7 -> 'a'-'h'
        ep.push_back(static_cast<char>('a' + enPassant.file()));
        // rank: 0-7 -> '8'-'1' (Board convention: rank 0 = row 8)
        ep.push_back(static_cast<char>('8' - enPassant.rank()));
        return ep;
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


}
