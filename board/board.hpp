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

    // ============================================
    // BOARD CONSTANTS - Square indices e castling
    // ============================================
    enum CastlingBits : uint8_t {
        WHITE_KINGSIDE  = 0,  // Bit 0 in castle bitmask
        WHITE_QUEENSIDE = 1,  // Bit 1
        BLACK_KINGSIDE  = 2,  // Bit 2
        BLACK_QUEENSIDE = 3   // Bit 3
    };
    
    // Starting square indices (Coords convention: a8=0, h1=63)
    static constexpr uint8_t WHITE_KING_START  = 60;  // e1
    static constexpr uint8_t BLACK_KING_START  = 4;   // e8
    static constexpr uint8_t WHITE_ROOK_A_START = 56;  // a1
    static constexpr uint8_t WHITE_ROOK_H_START = 63;  // h1
    static constexpr uint8_t BLACK_ROOK_A_START = 0;   // a8
    static constexpr uint8_t BLACK_ROOK_H_START = 7;   // h8
    
    static constexpr uint8_t CASTLING_RIGHTS_ALL = 0x0F; // All 4 castling rights

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

    // ============================================
    // COLOR UTILITIES - Branchless operations
    // ============================================
    
    // Branchless color flip: WHITE (0x0) <-> BLACK (0x8)
    static constexpr uint8_t oppositeColor(uint8_t color) noexcept { return color ^ 0x8; }
    
    // Branchless color to array index: WHITE (0x0) -> 0, BLACK (0x8) -> 1
    static constexpr uint8_t colorToIndex(uint8_t color) noexcept { return color >> 3; }
    
    // Convert bool color to array index: true (white) -> 0, false (black) -> 1
    static constexpr int colorBoolToIndex(bool isWhite) noexcept { return isWhite ? 0 : 1; }
    
    // ============================================
    // RANK UTILITIES - Color-dependent rank helpers
    // ============================================
    
    // Get promotion rank for a color (Coords convention: rank 0 = 8th rank, rank 7 = 1st rank):
    // WHITE pawns promote on the top of the board (rank 0), BLACK pawns promote on bottom (rank 7).
    template<bool IsWhite>
    static constexpr uint8_t promotionRank() noexcept { return IsWhite ? 0 : 7; }

    // Runtime version of promotion rank
    static constexpr uint8_t promotionRank(bool isWhite) noexcept { return isWhite ? 0 : 7; }
    
    // Check if a rank is a promotion rank for a given color
    static constexpr bool isPromotionRank(uint8_t rank, bool isWhite) noexcept { return rank == promotionRank(isWhite); }
    
    // Get back rank (starting rank) for a color: WHITE -> 0 (1st rank), BLACK -> 7 (8th rank)
    template<bool IsWhite>
    static constexpr uint8_t backRank() noexcept { return IsWhite ? 0 : 7; }
    
    // Get seventh rank for a color: WHITE -> 6 (7th rank), BLACK -> 1 (2nd rank)
    template<bool IsWhite>
    static constexpr uint8_t seventhRank() noexcept { return IsWhite ? 6 : 1; }
    
    // ============================================
    // PIECE-CHAR LOOKUP TABLES - Compile-time
    // ============================================
    
    // ASCII character -> piece_id lookup (256 entries, mostly EMPTY)
    static constexpr std::array<uint8_t, 256> CHAR_TO_PIECE_TYPE = []() {
        std::array<uint8_t, 256> table{};
        for (int i = 0; i < 256; ++i) table[i] = EMPTY;
        
        table['P'] = PAWN;   table['p'] = PAWN | BLACK;
        table['N'] = KNIGHT; table['n'] = KNIGHT | BLACK;
        table['B'] = BISHOP; table['b'] = BISHOP | BLACK;
        table['R'] = ROOK;   table['r'] = ROOK | BLACK;
        table['Q'] = QUEEN;  table['q'] = QUEEN | BLACK;
        table['K'] = KING;   table['k'] = KING | BLACK;
        
        return table;
    }();
    
    // piece_id -> ASCII character lookup (8 entries: EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING)
    static constexpr std::array<char, 8> PIECE_TYPE_TO_CHAR = {
        '.', 'P', 'N', 'B', 'R', 'Q', 'K', '?'
    };


    
    // ============================================
    // SQUARE TRANSFORMATIONS - Geometric operations
    // ============================================
    
    // Vertical mirror: flip rank (a1 <-> a8, b1 <-> b8, etc.)
    static constexpr uint8_t verticalMirror(uint8_t sq) noexcept {
        return sq ^ 56;  // XOR with 0b111000 flips bits 3-5 (rank)
    }
    
    // Horizontal mirror: flip file (a1 <-> h1, a2 <-> h2, etc.)
    static constexpr uint8_t horizontalMirror(uint8_t sq) noexcept {
        return sq ^ 7;   // XOR with 0b000111 flips bits 0-2 (file)
    }
    
    // ============================================
    // FILE/RANK MASKS - Precomputed bitboard masks
    // ============================================
    
    // File masks: vertical columns (a-h files)
    static constexpr uint64_t FILE_MASKS[8] = {
        0x0101010101010101ULL,  // a-file
        0x0202020202020202ULL,  // b-file
        0x0404040404040404ULL,  // c-file
        0x0808080808080808ULL,  // d-file
        0x1010101010101010ULL,  // e-file
        0x2020202020202020ULL,  // f-file
        0x4040404040404040ULL,  // g-file
        0x8080808080808080ULL,  // h-file
    };
    
    // Rank masks: horizontal rows (1-8 ranks)
    static constexpr uint64_t RANK_MASKS[8] = {
        0x00000000000000FFULL,  // rank 1
        0x000000000000FF00ULL,  // rank 2
        0x0000000000FF0000ULL,  // rank 3
        0x00000000FF000000ULL,  // rank 4
        0x000000FF00000000ULL,  // rank 5
        0x0000FF0000000000ULL,  // rank 6
        0x00FF000000000000ULL,  // rank 7
        0xFF00000000000000ULL,  // rank 8
    };
    
    // Get file mask for a given file (0-7)
    static constexpr uint64_t fileMask(int file) noexcept {
        return FILE_MASKS[file];
    }
    
    // Get rank mask for a given rank (0-7)
    static constexpr uint64_t rankMask(int rank) noexcept {
        return RANK_MASKS[rank];
    }
    
    // Get file mask for a square
    static constexpr uint64_t fileMaskFromSquare(uint8_t sq) noexcept {
        return FILE_MASKS[sq & 7];  // Extract file (bits 0-2)
    }
    
    // Get rank mask for a square
    static constexpr uint64_t rankMaskFromSquare(uint8_t sq) noexcept {
        return RANK_MASKS[sq >> 3];  // Extract rank (bits 3-5)
    }

    // Bit mask for individual squares (1ULL << sq)
    static constexpr std::array<uint64_t, 64> BIT_MASKS = []() constexpr {
        std::array<uint64_t, 64> masks{};
        for (int i = 0; i < 64; ++i) masks[i] = (1ULL << i);
        return masks;
    }();

    // Get bit mask for a specific square index
    static constexpr uint64_t bitMask(uint8_t sq) noexcept { return BIT_MASKS[sq]; }
    
    // ============================================
    // FILE/RANK EXTRACTORS - Replace raw bitwise ops
    // ============================================
    
    // Extract file from square index (0-7, representing a-h)
    static constexpr uint8_t fileOf(uint8_t sq) noexcept {
        return sq & 7;  // Extract bits 0-2
    }
    
    // Extract rank from square index (0-7, representing 8th-1st in Coords convention)
    static constexpr uint8_t rankOf(uint8_t sq) noexcept {
        return sq >> 3;  // Extract bits 3-5
    }

    struct Move {
        Coords from;
        Coords to;
        char promotionPiece = '\0'; // 'q', 'r', 'b', 'n' or '\0' for no promotion
        
        // Operatore di confronto per iterative deepening move ordering
        bool operator==(const Move& other) const noexcept {
            return from == other.from && to == other.to && promotionPiece == other.promotionPiece;
        }
        
        // Rotate custom ottimizzata: sposta moves[index] in moves[0]
        // e shifta moves[0..index-1] una posizione a destra
        // Esempio: rotate([A,B,C,D,E], 3) -> [D,A,B,C,E]
        template<typename MoveContainer>
        static void rotate(MoveContainer& moves, size_t index) noexcept {
            Move temp = moves[index];
            // Shifta tutte le mosse [0..index-1] una posizione a destra
            for (size_t i = index; i > 0; --i) {
                moves[i] = moves[i - 1];
            }
            moves[0] = temp;
        }
    };

    struct MoveState {
        // Game state before the move
        uint8_t  prevActiveColor{};
        uint16_t prevHalfMoveClock{};
        uint16_t prevFullMoveClock{};

        // En passant and castling rights
        Coords  prevEnPassant{};
        uint8_t prevCastle{};   // bitmask copy of castle
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
        // uint8_t prevWhiteKingIndex{64};
        // uint8_t prevBlackKingIndex{64};
    };

    // CODICE MORTO RIMOSSO: struct UndoInfo non era usata nel codebase

    Board() noexcept {
        fromFenToBoard(STARTING_FEN);
    }

    explicit Board(const std::array<uint32_t, 8>& chessboard) noexcept
        : chessboard(chessboard)
        , castle(CASTLING_RIGHTS_ALL)
        , enPassant() 
        , halfMoveClock(0)
        , fullMoveClock(1)
        , activeColor(WHITE)
    {
        this->updateOccupancyBB();
    }

    explicit Board(const std::string& fen) {
        fromFenToBoard(fen);
    }
   
    //! GETTERS
    // Primary getter: works directly with index (most efficient)
    __attribute__((hot, always_inline))
    constexpr inline uint8_t get(uint8_t index) const noexcept {
        const uint8_t rank = index >> 3;  // index / 8 (Coords convention)
        const uint8_t file = index & 7;   // index % 8
        // Convert from Coords convention to Board storage
        return (chessboard[7 - rank] >> (file << 2)) & MASK_PIECE;
    }
    
    // Convenience getter: from Coords object
    __attribute__((always_inline))
    constexpr inline uint8_t get(Coords coords) const noexcept {
        return this->get(coords.index);
    }
    
    // Direct storage access getter (bypasses Coords convention)
    __attribute__((always_inline))
    constexpr inline uint8_t get(uint8_t row, uint8_t col) const noexcept {
        return (chessboard[row] >> (col << 2)) & MASK_PIECE;
    }
    
    // String notation getter
    uint8_t get(const std::string& square) const noexcept { 
        const uint8_t col = square[0] - 'a';
        const uint8_t row = square[1] - '1';
        return this->get(row, col);
    }
    
    std::string getCurrentFen() const noexcept { return this->fromBoardToFen(); };

    // TODO check whether Castle and HasMoved getters works fine :D
    constexpr uint8_t getActiveColor() const noexcept { return this->activeColor; }

    // Castling rights (KQkq) and hasMoved flags stored as bitmasks in uint8_t
    // castle bits: 0=white king side (K), 1=white queen side (Q), 2=black king side (k), 3=black queen side (q)
    constexpr bool getCastle(uint8_t index) const noexcept {
        return (castle & (1u << index));
    }

    // hasMoved bits: K, Ra, Rh, k, ra, rh  (use same ordering as old vector<bool>)
    constexpr bool getHasMoved(uint8_t index) const noexcept {
        return (hasMoved & (1u << index));
    }

    // Both ways to get color of piece at position
    __attribute__((always_inline))
    constexpr inline uint8_t getColor(const Coords& pos) const noexcept {
        const uint8_t rawPiece = this->get(pos);
        if ((rawPiece & MASK_PIECE_TYPE) == EMPTY) [[unlikely]] {
            return EMPTY;
        }
        return (rawPiece & MASK_COLOR) ? BLACK : WHITE;
    }

    __attribute__((always_inline))
    constexpr inline uint8_t getColor(uint8_t index) const noexcept {
        const uint8_t rawPiece = this->get(index);
        if ((rawPiece & MASK_PIECE_TYPE) == EMPTY) [[unlikely]] {
            return EMPTY;
        }
        return (rawPiece & MASK_COLOR) ? BLACK : WHITE;
    }

    constexpr uint16_t getHalfMoveClock() const noexcept { return halfMoveClock; }
    constexpr uint16_t getFullMoveClock() const noexcept { return fullMoveClock; }

    //! SETTERS
    // Primary setter: works directly with index (most efficient)
    __attribute__((hot, always_inline))
    inline void set(uint8_t index, piece_id value) noexcept {
        const uint8_t rank = index >> 3;
        const uint8_t file = index & 7;
        const uint8_t internal_row = 7 - rank;
        const uint8_t shift = file << 2; // file * 4
        chessboard[internal_row] = (chessboard[internal_row] & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }
    
    // Convenience setter: from Coords object
    __attribute__((always_inline))
    inline void set(Coords coords, piece_id value) noexcept {
        this->set(coords.index, value);
    }

    // Direct storage access setter (bypasses Coords convention)
    __attribute__((always_inline))
    inline void set(uint8_t row, uint8_t col, piece_id value) noexcept {
        const uint8_t shift = col << 2; // col * 4
        chessboard[row] = (chessboard[row] & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }

    void setNextTurn() noexcept {
        if (activeColor == WHITE) {
            activeColor = BLACK;
        } else {
            activeColor = WHITE;        
            ++fullMoveClock;
        }
    }

    void setPrevTurn() noexcept {
        if (this->activeColor == BLACK) {
            this->activeColor = WHITE;
        } else {
            this->activeColor = BLACK;
            if (this->fullMoveClock > 1) {
                this->fullMoveClock--;
            }
        }
        if (this->halfMoveClock > 0) {
            this->halfMoveClock--;
        }
    }

    //! Operator overloads
    constexpr uint8_t operator[](const Coords& coords) const noexcept { return this->get(coords); }
    uint8_t operator[](const Coords& coords) noexcept { return this->get(coords); }
    constexpr uint8_t operator[](uint8_t index) const noexcept { return this->get(index); } // assert index 0-63
    uint8_t operator[](uint8_t index) noexcept { return this->get(index); }
    constexpr bool operator==(const Board& other) const noexcept { return this->chessboard == other.chessboard; }
    constexpr bool operator!=(const Board& other) const noexcept { return this->chessboard != other.chessboard; }


    //! PER DEBUG
    static constexpr size_t CHESSBOARD_SIZE() noexcept { return sizeof(chessboard); } // 32 byte
    // static constexpr size_t BOARD_SIZE(Board b) noexcept { return sizeof(b); }

    // Iterator support
    auto begin() noexcept { return chessboard.begin(); }
    auto end() noexcept { return chessboard.end(); }
    constexpr auto begin() const noexcept { return chessboard.begin(); }
    constexpr auto end() const noexcept { return chessboard.end(); }
    constexpr auto cbegin() const noexcept { return chessboard.cbegin(); }
    constexpr auto cend() const noexcept { return chessboard.cend(); }


    // Piece movement logic
    constexpr bool isSameColor(const Coords& pos1, const Coords& pos2) const noexcept {
        uint8_t p1 = this->get(pos1);
        uint8_t p2 = this->get(pos2);
        if (p1 == EMPTY || p2 == EMPTY) return false;
        return (p1 & BLACK) == (p2 & BLACK);
    }


    // CRITICAL OPTIMIZATION: Inline updateChessboard to avoid double get/set overhead
    // OLD: 3 function calls (1 get + 2 set) -> 6 index conversions
    // NEW (commented): Direct inline -> 0 function calls, direct array access
    __attribute__((always_inline))
    inline void updateChessboard(const Coords& from, const Coords& to, piece_id piece) noexcept {
        this->set(to, piece);
        this->set(from, EMPTY);
/*
        const uint8_t fromIndex = from.index;
        const uint8_t toIndex = to.index;
        
        // Direct array access usando index (evita conversioni ripetute)
        const uint8_t fromRank = fromIndex >> 3;
        const uint8_t fromFile = fromIndex & 7;
        const uint8_t toRank = toIndex >> 3;
        const uint8_t toFile = toIndex & 7;
        
        const uint8_t fromRow = 7 - fromRank;
        const uint8_t toRow = 7 - toRank;
        
        const uint8_t fromShift = fromFile << 2;
        const uint8_t toShift = toFile << 2;
        
        // Get piece from source (1 array access)
        const uint8_t piece = (chessboard[fromRow] >> fromShift) & MASK_PIECE;
        
        // Clear source and set destination (2 array writes)
        chessboard[fromRow] = (chessboard[fromRow] & ~(MASK_PIECE << fromShift));
        chessboard[toRow] = (chessboard[toRow] & ~(MASK_PIECE << toShift)) | ((piece & MASK_PIECE) << toShift);
  */
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

        // Single loop: iterate all 64 squares directly
        // index = rank * 8 + file, where rank 0 = row 8, rank 7 = row 1
        for (uint8_t index = 0; index < 64; ++index) {
            const uint8_t piece = this->get(index);
            
            if (piece == EMPTY) continue;
            
            const uint64_t bit = bitMask(index);
            const uint8_t color = piece >> 3; // Extract color bit directly (bit 3)

            occupancy |= bit;

            switch (piece & MASK_PIECE_TYPE) {
                case PAWN:   pawns_bb[color]   |= bit; break;
                case KNIGHT: knights_bb[color] |= bit; break;
                case BISHOP: bishops_bb[color] |= bit; break;
                case ROOK:   rooks_bb[color]   |= bit; break;
                case QUEEN:  queens_bb[color]  |= bit; break;
                case KING:   kings_bb[color]   |= bit; break;
            }
        }
    }

    __attribute__((always_inline))
    void fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept {
        this->occupancy |= (bitMask(toIndex));  // Set the bit at 'to' position    
        this->occupancy &= ~(bitMask(fromIndex)); // Clear the bit at 'from' position
    }

    __attribute__((always_inline))
    void addPieceToBitboards(uint8_t piece, uint8_t index) noexcept {
        if (piece == EMPTY) return;
        const uint8_t color = (piece & MASK_COLOR) != 0; // BLACK=1, WHITE=0
        const uint64_t bit = (bitMask(index));
        switch (piece & MASK_PIECE_TYPE) {
            case PAWN:   pawns_bb[color]   |= bit; break;
            case KNIGHT: knights_bb[color] |= bit; break;
            case BISHOP: bishops_bb[color] |= bit; break;
            case ROOK:   rooks_bb[color]   |= bit; break;
            case QUEEN:  queens_bb[color]  |= bit; break;
            case KING:   kings_bb[color]   |= bit; break;
        }
    }

    __attribute__((always_inline))
    void removePieceFromBitboards(uint8_t piece, uint8_t index) noexcept {
        if (piece == EMPTY) return;
        const uint8_t color = (piece & MASK_COLOR) != 0;
        const uint64_t mask = ~(bitMask(index));
        switch (piece & MASK_PIECE_TYPE) {
            case PAWN:   pawns_bb[color]   &= mask; break;
            case KNIGHT: knights_bb[color] &= mask; break;
            case BISHOP: bishops_bb[color] &= mask; break;
            case ROOK:   rooks_bb[color]   &= mask; break;
            case QUEEN:  queens_bb[color]  &= mask; break;
            case KING:   kings_bb[color]   &= mask; break;
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
    bool canMoveToBB(const Coords& from, const Coords& to, bool inCheck) const noexcept;
    // ------------------------------------------------------------
    // CHECK / CHECKMATE / STALEMATE UTILITIES
    // ------------------------------------------------------------
    // Returns true if square 'targetIndex' is attacked by 'byColor'
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept;
    // Version that excludes a specific square from occupancy (for king move validation)
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept;
    // Optimized: check if ALL squares in mask are safe (not attacked) - for castling
    bool isCastlePathSafe(uint64_t squaresMask, uint8_t byColor) const noexcept;
    // Is the given color currently in check?
    bool inCheck(uint8_t color) const noexcept;
    // Does the color have at least one legal move?
    bool hasAnyLegalMove(uint8_t color) const noexcept;

    __attribute__((hot))
    bool isCheckmate(uint8_t color) const noexcept {return inCheck(color) && !hasAnyLegalMove(color);}

    bool isStalemate(uint8_t color) const noexcept {return !inCheck(color) && !hasAnyLegalMove(color);}

    // 50-move rule: draw if no pawn move or capture in the last 50 full moves (100 half-moves)
    bool isFiftyMoveRule() const noexcept { return halfMoveClock >= 100; }

    // General draw check: stalemate OR 50-move rule
    // Note: insufficient material and threefold repetition are not yet implemented
    bool isDraw(uint8_t color) const noexcept {
        return isStalemate(color) || isFiftyMoveRule();
    }

    void fromFenToBoard(const std::string& fen);

    std::string fromBoardToFen() const;

    // Ritorna la casa di en-passant, se esiste.
    // Se non esiste, ritorna una Coords "vuota" (rank e file -1).
    Coords getEnPassant() const noexcept {
        return enPassant;
    }

    // ============================================
    // BITBOARDS - Public per prestazioni critiche
    // Accesso diretto richiesto da Engine/MoveValidator in loop hot
    // Convenzione: [0] = WHITE, [1] = BLACK
    // ============================================
    std::array<uint64_t, 2> pawns_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> knights_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> bishops_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> rooks_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> queens_bb  = {0ULL, 0ULL};
    std::array<uint64_t, 2> kings_bb   = {0ULL, 0ULL};

private:
    // ============================================
    // BOARD STATE - Rappresentazione principale
    // ============================================
    board chessboard; // 8 * 32 bit = 256 bit = 32 byte

    // ============================================
    // GAME STATE - Stato della partita
    // ============================================
    uint8_t  castle = CASTLING_RIGHTS_ALL;  // Castling rights (KQkq) - 4 bits
    uint8_t  hasMoved = 0x00;               // Piece movement tracking - 6 bits
    Coords   enPassant{};                   // En-passant target square
    uint16_t halfMoveClock = 0;             // 50-move rule counter
    uint16_t fullMoveClock = 1;             // Current move number
    uint8_t  activeColor = WHITE;           // Current side to move
    
    std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    
    uint64_t occupancy = 0ULL;              // Combined occupancy bitboard
    // uint8_t whiteKingIndex = 64; // cache king squares for faster inCheck/isSquareAttacked
    // uint8_t blackKingIndex = 64;

    // ============================================
    // HELPER FUNCTIONS FOR canMoveToBB
    // ============================================
    
    // Lazy double-check detection (called only when inChk=true && fromType != KING)
    [[nodiscard]] inline bool isDoubleCheck(uint8_t movingColor) const noexcept;
    
    // Pawn move validation with en-passant optimization
    [[nodiscard]] inline bool isPawnMoveLegal(
        uint8_t fromIndex, 
        uint8_t toIndex,
        uint64_t toBit,
        uint8_t movingColor,
        uint8_t destPiece,
        uint8_t destColor
    ) const noexcept;
    
    // En passant move validation
    [[nodiscard]] inline bool isPawnEnPassantLegal(
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t movingColor
    ) const noexcept;
    
    // Simple piece pseudo-legal check (Knight/Bishop/Rook/Queen)
    [[nodiscard]] static inline bool isSimplePieceLegal(uint64_t bitMap, uint64_t toBit) noexcept;
    
    // King move validation (normal moves + castling delegation)
    [[nodiscard]] inline bool isKingMoveLegal(
        uint8_t fromIndex,
        uint8_t toIndex,
        uint64_t toBit,
        uint8_t movingColor
    ) const noexcept;
    
    // Castling dispatcher
    [[nodiscard]] inline bool canCastleToSquare(uint8_t fromIndex, uint8_t toIndex, uint8_t movingColor) const noexcept;
    
    // Generic castling validation (consolidated logic)
    [[nodiscard]] inline bool canCastleGeneric(bool isWhite, uint8_t fromIndex, bool isKingside) const noexcept;
    
    // Kingside castling validation
    [[nodiscard]] inline bool canCastleKingside(bool isWhite, uint8_t fromIndex) const noexcept;
    
    // Queenside castling validation
    [[nodiscard]] inline bool canCastleQueenside(bool isWhite, uint8_t fromIndex) const noexcept;
    
    // King safety check for non-king, non-pawn pieces
    [[nodiscard]] inline bool verifyKingSafetyForSimplePiece(
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t movingColor,
        uint8_t destPiece,
        uint8_t destColor
    ) const noexcept;

    // Helper: check if king at kingSq is attacked by byColor using custom bitboards
    bool isKingAttackedCustom(uint8_t kingSq, uint8_t byColor, uint64_t occ,
                              uint64_t pawns, uint64_t knights, uint64_t bishops,
                              uint64_t rooks, uint64_t queens, uint64_t kings) const noexcept;

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
