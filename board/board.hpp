#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>
#include <cctype>
#include <sstream>
#include <algorithm>
#include <cstddef>

#include "../coords/coords.hpp"
#include "../piece/pieces.hpp" // bitmap utilities

namespace chess {

using board = std::array<uint32_t, 8>;



class Board {

public:

    static constexpr uint8_t MASK_PIECE = 0x0F;      // 0000 1111
    static constexpr uint8_t MASK_COLOR = 0x08;      // 0000 1000
    static constexpr uint8_t MASK_PIECE_TYPE = 0x07; // 0000 0111

    enum piece_id : uint8_t {
    // piece bits
    EMPTY  = 0x0, // 0000 
    PAWN   = 0x1, // 0001
    KNIGHT = 0x2, // 0010
    BISHOP = 0x3, // 0011
    ROOK   = 0x4, // 0100
    QUEEN  = 0x5, // 0101
    KING   = 0x6, // 0110
    // color bit
    BLACK  = 0x8, // 1000
    WHITE  = 0x0, // 0000

    // ENPASSANT = 0x7  // 0111
    };

private:
    board chessboard; // 8 * 32 bit = 256 bit = 32 byte

    std::vector<bool> castle = {true, true, true, true};
    std::vector<bool> hasMoved = {false, false, false, false, false, false}; // K Ra Rh, k ra rh
    // uint8_t castle = 0x0F; // 4 bit for castling rights (KQkq) // 0000 1111 = all castling rights available // 1111=0x0F
    // uint8_t hasMoved = 0; // 3 bits to track king and rooks, 1 bit for spacing (K Ra Rh, k ra rh) = 0111 0111

    std::array<Coords, 2> enPassant = {Coords{}, Coords{}}; // WHITE and BLACK
    uint8_t halfMoveClock = 0; // Tracks the number of half-moves since the last pawn move or capture
    uint8_t fullMoveClock = 1; // Tracks the number of full moves in the game
    uint8_t activeColor = WHITE; // Tracks the active color (white or black)



    // std::unordered_map<std::tuple<piece_id, Coords>, std::vector<chess::Coords>> legalMoves; //? maybe there's a better way?

    std::string startingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";


    uint64_t occupancy = 0; // 64 bits to represent presence of pieces on the board
    
    

public:

    Board() noexcept {
        fromFenToBoard(startingFen);
    }

    Board(const std::array<uint32_t, 8>& chessboard) noexcept
        : chessboard(chessboard)
        , castle(this->MASK_PIECE) // 0x0F = 0000 1111 => all castling rights available
        , enPassant({Coords(), Coords()}) 
        , halfMoveClock(0)
        , fullMoveClock(1)
        , activeColor(WHITE)
    {
        this->updateOccupancyBB();
    }

    Board(const std::string& fen) {
        fromFenToBoard(fen);
    }
   
    //! GETTERS
    // assert(col <= 7)
    // assert(row <= 7)
    uint8_t get(Coords coords) const noexcept { return (chessboard.at(coords.rank) >> (coords.file * 4)) & this->MASK_PIECE; }
    constexpr uint8_t get(uint8_t row, uint8_t col) const noexcept { return (chessboard.at(row) >> (col * 4)) & this->MASK_PIECE; }
    
    uint8_t getByNoteCoords(const std::string& square) const noexcept { // TODO method name to "get()"
      // Debug now
      uint8_t col = square.at(0) - 'a';
      uint8_t row = square.at(1) - '1';

      return this->get(row, col);
    }
    
    std::string getCurrentFen() const noexcept { return this->fromBoardToFen(); };

    // TODO check whether Castle and HasMoved getters works fine :D
    uint8_t getActiveColor() const noexcept { return this->activeColor; }
    bool getCastle(const uint8_t index) const noexcept { return this->castle.at(index); }
    bool getHasMoved(const uint8_t index) const noexcept { return this->hasMoved.at(index); }
    // uint8_t getCastle() const noexcept { return this->castle; }
    // uint8_t getWhiteCastleRights() const noexcept { return (this->castle >> 2) & 0x03; } // KQ
    // uint8_t getBlackCastleRights() const noexcept { return this->castle & 0x03; } // kq
    // uint8_t getHasMoved() const noexcept { return this->hasMoved; }
    // uint8_t getWhiteHasMoved() const noexcept { return (this->hasMoved >> 4) & 0x07; } // K Ra Rh
    // uint8_t getBlackHasMoved() const noexcept { return this->hasMoved & 0x07; } // k ra rh


    // Both ways to get color of piece at position
    uint8_t getColor(const Coords& pos) const noexcept {
        const piece_id rawPiece = static_cast<piece_id>((chessboard.at(pos.rank) >> (pos.file * 4)) & MASK_PIECE); // this->get(pos);
        if ((rawPiece & MASK_PIECE_TYPE) == EMPTY) {
            return EMPTY;
        }
        return (rawPiece & MASK_COLOR) != 0 ? BLACK : WHITE;
    }

    uint8_t getColor(uint8_t index) const noexcept {
        const uint8_t rank = static_cast<uint8_t>(index / 8);
        const uint8_t file = static_cast<uint8_t>(index % 8);
        const uint8_t rawPiece = static_cast<uint8_t>((chessboard.at(rank) >> (file * 4)) & MASK_PIECE);
        if ((rawPiece & MASK_PIECE_TYPE) == EMPTY) {
            return EMPTY;
        }
        return (rawPiece & MASK_COLOR) != 0 ? BLACK : WHITE;
    }

    //! SETTERS
    void set(Coords coords, piece_id value) noexcept {
        const uint8_t shift = coords.file * 4;
        chessboard.at(coords.rank) = (chessboard.at(coords.rank) & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }

    void set(uint8_t row, uint8_t col, piece_id value) noexcept {
        const uint8_t shift = col * 4;
        chessboard.at(row) = (chessboard.at(row) & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }

    void set_linear(uint8_t index, piece_id value) noexcept { this->set(index % 8, index / 8, value); }
    
    // void setCastle(uint8_t value) noexcept { this->castle = value; }
    // void setHasMoved(uint8_t value) noexcept { this->hasMoved = value; }

    void setNextTurn() noexcept {
        this->activeColor = (this->activeColor == WHITE) ? BLACK : WHITE;
    }

    //! Operator overloads
    uint8_t operator[](const Coords& coords) const noexcept { return this->get(coords); }
    uint8_t operator[](const Coords& coords) noexcept { return this->get(coords); }
    uint8_t operator[](uint8_t index) const noexcept { return this->get(index % 8, index / 8); } // assert index 0-63 r
    uint8_t operator[](uint8_t index) noexcept { return this->get(index % 8, index / 8); }
    bool operator==(const Board& other) const noexcept { return this->chessboard == other.chessboard; }
    bool operator!=(const Board& other) const noexcept { return this->chessboard != other.chessboard; }

    /*
    constexpr const std::array<uint32_t, 8>& chessboard() const noexcept {
        return chessboard;
    }
    
    void chessboard(const std::array<uint32_t, 8>& chessboard) noexcept {
        chessboard = chessboard;
    }*/
     
    void clear() noexcept { 
        chessboard.fill(0); 
    }
    
    //! PER DEBUG
    static constexpr size_t size() noexcept { return sizeof(chessboard); } // 32 byte

    // Iterator support
    auto begin() noexcept { return chessboard.begin(); }
    auto end() noexcept { return chessboard.end(); }
    constexpr auto begin() const noexcept { return chessboard.begin(); }
    constexpr auto end() const noexcept { return chessboard.end(); }
    constexpr auto cbegin() const noexcept { return chessboard.cbegin(); }
    constexpr auto cend() const noexcept { return chessboard.cend(); }


    // Piece movement logic
    bool isSameColor(const Coords& pos1, const Coords& pos2) const noexcept {
        uint8_t p1 = this->get(pos1);
        uint8_t p2 = this->get(pos2);
        if (p1 == EMPTY || p2 == EMPTY) return false;
        return (p1 & BLACK) == (p2 & BLACK);
    }




    //! GET MOVE BY BITBOARD
    //!
    //! GET MOVE BY BITBOARD
    //!
    //! GET MOVE BY BITBOARD

    uint64_t getPiecesBitMap() const noexcept {
        uint64_t bitMap = 0;
        for (uint8_t rank = 0; rank < 8; ++rank) {
            for (uint8_t file = 0; file < 8; ++file) {
                uint8_t piece = this->get(rank, file);
                if (piece != EMPTY) {
                    uint8_t index = rank * 8 + file;
                    bitMap |= (1ULL << index);
                }
            }
        }
        return bitMap;
    }

    void updateOccupancyBB() noexcept {
        this->occupancy = this->getPiecesBitMap();
    }

    void fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept {
        occupancy |= (1ULL << toIndex);  // Set the bit at 'to' position    
        occupancy &= ~(1ULL << fromIndex); // Clear the bit at 'from' position
    }

    bool moveBB(const Coords& from, const Coords& to) noexcept {        
        if (!canMoveToBB(from, to))
            return false;

        piece_id piece = static_cast<piece_id>(this->get(from));

        this->set(to, piece);
        this->set(from, EMPTY);

        // updateOccupancyBB();
        fastUpdateOccupancyBB(from.toIndex(), to.toIndex());

        this->setNextTurn();
        return true;
    }

    bool canMoveToBB(const Coords& from, const Coords& to) const noexcept {
        uint64_t bitMap;

        uint8_t fromIndex = from.toIndex();
        uint8_t toIndex = to.toIndex();

        switch (this->get(from) & this->MASK_PIECE_TYPE) { // Mask to get piece type only
            case PAWN:
                bitMap = pieces::getPawnAttacks(fromIndex, (this->getColor(from) == WHITE))
                       | pieces::getPawnForwardPushes(fromIndex, (this->getColor(from) == WHITE), bitMap);
                break;
            case KNIGHT:
                bitMap = pieces::getKnightAttacks(fromIndex);
                break;
            case BISHOP:
                bitMap = pieces::getBishopAttacks(fromIndex, bitMap);
                break;
            case ROOK:
                bitMap = pieces::getRookAttacks(fromIndex, bitMap);
                break;
            case QUEEN:
                bitMap = pieces::getQueenAttacks(fromIndex, bitMap);
                break;
            case KING:
                bitMap = pieces::getKingAttacks(fromIndex);
                break;
            default:
                break;
        }

        // se Coords to è dentro get_Attacks allora return true:
        return (bitMap & (1ULL << toIndex));
    }













    void fromFenToBoard(const std::string& fen) {
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

    std::string fromBoardToFen() const {
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

};

} // namespace chess

#endif
