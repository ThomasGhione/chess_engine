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
#include "../piece/piece.hpp" // bitmap utilities


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


    struct Move {
        Coords from;
        Coords to;
    };

    struct MoveState {
        // Game state before the move
        uint8_t  prevActiveColor{};
        uint16_t prevHalfMoveClock{};
        uint16_t prevFullMoveClock{};

        // En passant and castling rights
        std::array<Coords, 2> prevEnPassant{};
        uint8_t               prevCastle{};   // bitmask copy of castle
        uint8_t               prevHasMoved{}; // bitmask copy of hasMoved

        // Piece information related to the move
        uint8_t capturedPiece{};        // piece captured on destination or via en-passant (0 if none)
        uint8_t fromPiece{};            // original piece on "from" before the move (useful for promotions)
        uint8_t promotionPieceType{};   // non-zero if the move is a promotion (PAWN=0 -> no promotion)

        bool    wasEnPassantCapture{};  // true if the move was an en-passant capture
        uint8_t enPassantCapturedIndex{}; // index (0..63) of the pawn captured en-passant

        bool    wasCastling{};          // true if the move was a castling move
        uint8_t rookFromIndex{};        // rook start square index in castling
        uint8_t rookToIndex{};          // rook destination square index in castling

        // Optional cached king indices (if used elsewhere)
        uint8_t prevWhiteKingIndex{64};
        uint8_t prevBlackKingIndex{64};
    };

    struct UndoInfo {
        MoveState state;
    };


    Board() noexcept {
        fromFenToBoard(STARTING_FEN);
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
    inline uint8_t get(uint8_t row, uint8_t col) const noexcept {
        return (chessboard[row] >> (col << 2)) & MASK_PIECE;
    }    
    inline uint8_t get(Coords coords) const noexcept {
        return this->get(coords.rank, coords.file);
    }
    inline uint8_t get(uint8_t index) const noexcept {
        const uint8_t rank = index >> 3;  // index / 8
        const uint8_t file = index & 7;   // index % 8
        return this->get(rank, file);
    }
    uint8_t get(const std::string& square) const noexcept { 
        const uint8_t col = square[0] - 'a';
        const uint8_t row = square[1] - '1';
        return this->get(row, col);
    }
    
    std::string getCurrentFen() const noexcept { return this->fromBoardToFen(); };

    // TODO check whether Castle and HasMoved getters works fine :D
    uint8_t getActiveColor() const noexcept { return this->activeColor; }

    // Castling rights (KQkq) and hasMoved flags stored as bitmasks in uint8_t
    // castle bits: 0=white king side (K), 1=white queen side (Q), 2=black king side (k), 3=black queen side (q)
    bool getCastle(uint8_t index) const noexcept {
        return (castle & (1u << index));
    }

    // hasMoved bits: K, Ra, Rh, k, ra, rh  (use same ordering as old vector<bool>)
    bool getHasMoved(uint8_t index) const noexcept {
        return (hasMoved & (1u << index));
    }

    // Both ways to get color of piece at position
    uint8_t getColor(const Coords& pos) const noexcept {
        const uint8_t rawPiece = this->get(pos);
        if ((rawPiece & MASK_PIECE_TYPE) == EMPTY) {
            return EMPTY;
        }
        return (rawPiece & MASK_COLOR) ? BLACK : WHITE;
    }

    uint8_t getColor(uint8_t index) const noexcept {
        const uint8_t rawPiece = this->get(index);
        if ((rawPiece & MASK_PIECE_TYPE) == EMPTY) {
            return EMPTY;
        }
        return (rawPiece & MASK_COLOR) ? BLACK : WHITE;
    }

    uint16_t getHalfMoveClock() const noexcept { return halfMoveClock; }
    uint16_t getFullMoveClock() const noexcept { return fullMoveClock; }

    //! SETTERS
    void set(Coords coords, piece_id value) noexcept {
        const uint8_t shift = coords.file * 4;
        chessboard[coords.rank] = (chessboard[coords.rank] & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }

    void set(uint8_t row, uint8_t col, piece_id value) noexcept {
        const uint8_t shift = col * 4;
        chessboard[row] = (chessboard[row] & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }

    void setNextTurn() noexcept {
        // this->activeColor = (this->activeColor == WHITE) ? BLACK : WHITE;
        if (this->activeColor == WHITE) {
            this->activeColor = BLACK;
        } else {
            this->activeColor = WHITE;
            this->fullMoveClock++;
        }
        this->halfMoveClock++;
    }

    //! Operator overloads
    uint8_t operator[](const Coords& coords) const noexcept { return this->get(coords); }
    uint8_t operator[](const Coords& coords) noexcept { return this->get(coords); }
    uint8_t operator[](uint8_t index) const noexcept { return this->get(index); } // assert index 0-63
    uint8_t operator[](uint8_t index) noexcept { return this->get(index); }
    bool operator==(const Board& other) const noexcept { return this->chessboard == other.chessboard; }
    bool operator!=(const Board& other) const noexcept { return this->chessboard != other.chessboard; }


    //! PER DEBUG
    static constexpr size_t CHESSBOARD_SIZE() noexcept { return sizeof(chessboard); } // 32 byte
    static constexpr size_t BOARD_SIZE(Board b) noexcept { return sizeof(b); }

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


    void updateChessboard(const Coords& from, const Coords& to) noexcept {
        piece_id piece = static_cast<piece_id>(this->get(from));
        this->set(to, piece);
        this->set(from, EMPTY);
    }


    //! GET MOVE BY BITBOARD
    //!
    //! GET MOVE BY BITBOARD
    //!
    //! GET MOVE BY BITBOARD

    uint64_t getPiecesBitMap() const noexcept {
        // Occupancy bitboard already tracks all non-empty squares.
        return occupancy;
    }

    void updateOccupancyBB() noexcept {
        // Reset all bitboards
        occupancy       = 0ULL;
        pawns_bb[0]     = pawns_bb[1]     = 0ULL;
        knights_bb[0]   = knights_bb[1]   = 0ULL;
        bishops_bb[0]   = bishops_bb[1]   = 0ULL;
        rooks_bb[0]     = rooks_bb[1]     = 0ULL;
        queens_bb[0]    = queens_bb[1]    = 0ULL;
        kings_bb[0]     = kings_bb[1]     = 0ULL;

        // Single pass over all 64 squares using the fast index-based getter
        for (uint8_t index = 0; index < 64; ++index) {
            const uint8_t piece = this->get(index);
            const uint8_t type  = piece & MASK_PIECE_TYPE;
            if (type == EMPTY) {
                continue;
            }

            const uint64_t bit = (1ULL << index);
            const uint8_t color = (piece & MASK_COLOR) != 0; // BLACK=1, WHITE=0

            occupancy |= bit;

            switch (type) {
                case PAWN:   pawns_bb[color]   |= bit; break;
                case KNIGHT: knights_bb[color] |= bit; break;
                case BISHOP: bishops_bb[color] |= bit; break;
                case ROOK:   rooks_bb[color]   |= bit; break;
                case QUEEN:  queens_bb[color]  |= bit; break;
                case KING:   kings_bb[color]   |= bit; break;
                default: break;
            }
        }
    }

    void fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept {
        this->occupancy |= (1ULL << toIndex);  // Set the bit at 'to' position    
        this->occupancy &= ~(1ULL << fromIndex); // Clear the bit at 'from' position
    }

    void addPieceToBitboards(uint8_t piece, uint8_t index) noexcept {
        if ((piece & MASK_PIECE_TYPE) == EMPTY) return;
        uint8_t color = (piece & MASK_COLOR) != 0; // BLACK=1, WHITE=0
        uint64_t bit = (1ULL << index);
        switch (piece & MASK_PIECE_TYPE) {
            case PAWN:   pawns_bb[color]   |= bit; break;
            case KNIGHT: knights_bb[color] |= bit; break;
            case BISHOP: bishops_bb[color] |= bit; break;
            case ROOK:   rooks_bb[color]   |= bit; break;
            case QUEEN:  queens_bb[color]  |= bit; break;
            case KING:   kings_bb[color]   |= bit; break;
            default: break;
        }
    }

    void removePieceFromBitboards(uint8_t piece, uint8_t index) noexcept {
        if ((piece & MASK_PIECE_TYPE) == EMPTY) return;
        uint8_t color = (piece & MASK_COLOR) != 0;
        uint64_t mask = ~(1ULL << index);
        switch (piece & MASK_PIECE_TYPE) {
            case PAWN:   pawns_bb[color]   &= mask; break;
            case KNIGHT: knights_bb[color] &= mask; break;
            case BISHOP: bishops_bb[color] &= mask; break;
            case ROOK:   rooks_bb[color]   &= mask; break;
            case QUEEN:  queens_bb[color]  &= mask; break;
            case KING:   kings_bb[color]   &= mask; break;
            default: break;
        }
    }

    bool moveBB(const Coords& from, const Coords& to) noexcept;
    // Promote a pawn at 'at' using the provided choice char: 'q','r','b','n' (case-insensitive).
    // Returns false if the piece is not a pawn on its promotion rank, otherwise promotes and returns true.
    bool promote(const Coords& at, char choice) noexcept;
    // Overload: execute move and, if a pawn reaches last rank, promote using provided choice
    bool moveBB(const Coords& from, const Coords& to, char promotionChoice) noexcept;

    // Incremental make/unmake move (no legality checks, no Board copy)
    void doMove(const Move& m, MoveState& state, char promotionChoice = 'q') noexcept;
    void undoMove(const Move& m, const MoveState& state) noexcept;
    bool canMoveToBB(const Coords& from, const Coords& to) const noexcept;
    // ------------------------------------------------------------
    // CHECK / CHECKMATE / STALEMATE UTILITIES
    // ------------------------------------------------------------
    // Returns true if square 'targetIndex' is attacked by 'byColor'
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept;
    // Version that excludes a specific square from occupancy (for king move validation)
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept;
    // Is the given color currently in check?
    bool inCheck(uint8_t color) const noexcept;
    // Does the color have at least one legal move?
    bool hasAnyLegalMove(uint8_t color) const noexcept;

    bool isCheckmate(uint8_t color) const noexcept {return inCheck(color) && !hasAnyLegalMove(color);}

    bool isStalemate(uint8_t color) const noexcept {return !inCheck(color) && !hasAnyLegalMove(color);}

    void fromFenToBoard(const std::string& fen);

    std::string fromBoardToFen() const;

    // Ritorna la casa di en-passant per il colore dato, se esiste.
    // Se non esiste, ritorna una Coords "vuota" (rank e file -1).
    Coords getEnPassant(uint8_t color) const noexcept {
        if (color > 1) {
            return Coords{Coords::INVALID_COORDS, Coords::INVALID_COORDS};
        }
        return enPassant[color];
    }

private:
    board chessboard; // 8 * 32 bit = 256 bit = 32 byte

public:
    // Per-piece, per-color bitboards to accelerate move generation and attack tests
    std::array<uint64_t, 2> pawns_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> knights_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> bishops_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> rooks_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> queens_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> kings_bb = {0ULL, 0ULL};

private:

    uint8_t castle = 0x0F; // castle: 4 bits (KQkq) -> 0000 1111 = all castling rights available
    uint8_t hasMoved = 0x00; // hasMoved: 6 bits (K, Ra, Rh, k, ra, rh)
    std::array<Coords, 2> enPassant = {Coords{}, Coords{}}; // en-passant square per WHITE e BLACK
    
    uint16_t halfMoveClock = 0; // Tracks the number of half-moves since the last pawn move or capture
    uint16_t fullMoveClock = 1; // Tracks the number of full moves in the game
    uint8_t activeColor = WHITE; // Tracks the active color (white or black)
    
    std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";


    uint64_t occupancy = 0; // 64 bits to represent presence of pieces on the board
    uint8_t whiteKingIndex = 64; // cache king squares for faster inCheck/isSquareAttacked
    uint8_t blackKingIndex = 64;

    //helper for fromFenToBoard
    uint8_t charToPiece(char symbol);
    bool parseBoardSection(const std::string& boardSection, std::array<uint32_t, 8>& parsedBoard);
    uint8_t parseActiveColor(const std::string& activeSection);
    std::vector<bool> parseCastling(const std::string& castlingSection);
    Coords parseEnPassant(const std::string& enPassantSection);
    uint8_t safeParseInt(const std::string& section, int min, int max, int defaultValue);
   
    //helper fot fromBoardToFen
    std::string boardToFenPieces() const;
    char pieceTypeToChar(uint8_t pieceType) const;
    std::string castlingToFen() const;
    std::string enPassantToFen() const;
}; // Class Board

} // namespace chess

#endif
