#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>
#include <cctype>
#include <sstream>
#include <algorithm>
#include <cstddef>

#include "./coords.hpp"
#include "./piece.hpp" // bitmap utilities

namespace chess {

using board = std::array<uint32_t, 8>;

class Board {
public:

    // Structs and enums start
    enum CastlingBits : uint8_t {
        WHITE_KINGSIDE  = 0,  // Bit 0 in castle bitmask
        WHITE_QUEENSIDE = 1,  // Bit 1
        BLACK_KINGSIDE  = 2,  // Bit 2
        BLACK_QUEENSIDE = 3   // Bit 3
    };

    // piece bits
    enum piece_id : uint8_t {
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
    };

    enum class MoveKind : uint8_t {
        Quiet = 0,
        Capture,
        DoublePawnPush,
        EnPassant,
        Castling,
        PromotionQuiet,
        PromotionCapture
    };

    struct Move {
        Coords from;
        Coords to;
        char promotionPiece = '\0'; // 'q', 'r', 'b', 'n' or '\0' for no promotion
        
        // Comparison operator for iterative deepening move ordering
        bool operator==(const Move& other) const noexcept;
        
        // Optimized custom rotate: move moves[index] to moves[0]
        // and shift moves[0..index-1] one position to the right
        // Example: rotate([A,B,C,D,E], 3) -> [D,A,B,C,E]
        template<typename MoveContainer>
        static void rotate(MoveContainer& moves, size_t index) noexcept;

        std::string toUCIString() const noexcept;
    };

    struct MoveState {
        uint16_t prevHalfMoveClock{};
        uint16_t prevFullMoveClock{};
        uint8_t  prevHistorySize{};
        uint64_t prevHistoryHead{};

        // Game state before the move
        uint8_t  prevActiveColor{};

        // En passant and castling rights
        Coords  prevEnPassant{};
        uint8_t prevCastle{};   // bitmask copy of castle
        uint8_t prevHasMoved{}; // bitmask copy of hasMoved

        // Piece information related to the move
        uint8_t capturedPiece{};        // piece captured on destination or via en-passant (0 if none)
        uint8_t fromPiece{};            // original piece on "from" before the move (useful for promotions)
        uint8_t promotionPieceType{};   // non-zero if the move is a promotion (PAWN=0 -> no promotion)

        bool    wasEnPassantCapture{};  // true if the move was an en-passant capture
        uint8_t enPassantCapturedIndex{}; // index (0..63) of the pawn captured en-passant

        bool    wasCastling{};          // true if the move was a castling move
        uint8_t rookFromIndex{};        // rook start square index in castling
        uint8_t rookToIndex{};          // rook destination square index in castling
        bool    historyWasReset{};      // true if repetition history was reset by the move
        MoveKind moveKind{MoveKind::Quiet};
    };
    // Structs and enums end
    
    // Constructors start
    Board() noexcept;
    explicit Board(const std::array<uint32_t, 8>& chessboard) noexcept;
    explicit Board(const std::string& fen);
    // Constructors end

    // Methods start
    static constexpr uint8_t oppositeColor(uint8_t color) noexcept;
    static constexpr uint8_t colorToIndex(uint8_t color) noexcept;
    static constexpr int colorBoolToIndex(bool isWhite) noexcept;
    static constexpr uint8_t promotionRank(bool isWhite) noexcept;
    static constexpr bool isPromotionRank(uint8_t rank, bool isWhite) noexcept;
    static constexpr uint8_t verticalMirror(uint8_t sq) noexcept;
    static constexpr uint8_t horizontalMirror(uint8_t sq) noexcept;
    static constexpr uint64_t fileMask(int file) noexcept;
    static constexpr uint64_t rankMask(int rank) noexcept;
    static constexpr uint64_t fileMaskFromSquare(uint8_t sq) noexcept;
    static constexpr uint64_t rankMaskFromSquare(uint8_t sq) noexcept;
    static constexpr uint64_t bitMask(uint8_t sq) noexcept;
    static constexpr uint8_t fileOf(uint8_t sq) noexcept;
    static constexpr uint8_t rankOf(uint8_t sq) noexcept;

    template<bool IsWhite>
    static constexpr uint8_t promotionRank() noexcept;

    template<bool IsWhite>
    static constexpr uint8_t backRank() noexcept;

    template<bool IsWhite>
    static constexpr uint8_t seventhRank() noexcept;

    __attribute__((hot, always_inline))
    constexpr inline uint8_t get(uint8_t index) const noexcept;
    
    __attribute__((always_inline))
    constexpr inline uint8_t get(Coords coords) const noexcept;
    
    __attribute__((always_inline))
    constexpr inline uint8_t get(uint8_t row, uint8_t col) const noexcept;

    __attribute__((always_inline))
    constexpr inline uint8_t getColor(const Coords& pos) const noexcept;

    __attribute__((always_inline))
    constexpr inline uint8_t getColor(uint8_t index) const noexcept;
    
    __attribute__((hot, always_inline))
    inline void set(uint8_t index, piece_id value) noexcept;

    __attribute__((always_inline))
    inline void set(Coords coords, piece_id value) noexcept;

    __attribute__((always_inline))
    inline void set(uint8_t row, uint8_t col, piece_id value) noexcept;

    __attribute__((always_inline))
    constexpr inline bool isSameColor(const Coords& pos1, const Coords& pos2) const noexcept;

    __attribute__((always_inline))
    inline void updateChessboard(const Coords& from, const Coords& to, piece_id piece) noexcept;

    __attribute__((always_inline))
    void fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept;

    __attribute__((always_inline))
    void addPieceToBB(uint8_t piece, uint8_t index) noexcept;

    __attribute__((always_inline))
    void removePieceFromBB(uint8_t piece, uint8_t index) noexcept;

    __attribute__((hot))
    bool isCheckmate(uint8_t color) const noexcept;

    uint8_t get(const std::string& square) const noexcept; 
    std::string getCurrentFen() const noexcept;
    constexpr uint8_t getActiveColor() const noexcept;
    constexpr bool getCastle(uint8_t index) const noexcept;
    constexpr bool getHasMoved(uint8_t index) const noexcept;
    constexpr uint16_t getHalfMoveClock() const noexcept;
    constexpr uint16_t getFullMoveClock() const noexcept;
    void setNextTurn() noexcept;
    void setPrevTurn() noexcept;
    
    constexpr uint8_t operator[](const Coords& coords) const noexcept;
    uint8_t operator[](const Coords& coords) noexcept;
    constexpr uint8_t operator[](uint8_t index) const noexcept; // assert index 0-63
    uint8_t operator[](uint8_t index) noexcept;
    constexpr bool operator==(const Board& other) const noexcept;
    constexpr bool operator!=(const Board& other) const noexcept;
    
    static constexpr size_t CHESSBOARD_SIZE() noexcept; // 32 byte
    
    uint64_t getPiecesBitMap() const noexcept;
    void updateOccupancyBB() noexcept;
    
    void doMove(const Move& m, MoveState& state, char promotionChoice = 'q') noexcept;
    void undoMove(const Move& m, const MoveState& state) noexcept;
    void doNullMove(MoveState& state) noexcept;
    void undoNullMove(const MoveState& state) noexcept;
    
    bool move(const Coords& from, const Coords& to) noexcept;
    bool move(const Coords& from, const Coords& to, char promotionChoice) noexcept;
    bool promote(const Coords& at, char choice) noexcept;
    bool isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, bool inCheck) const noexcept;
    bool isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, bool inCheck, bool inDoubleCheck) const noexcept;
    bool isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, uint8_t fromPiece, bool inCheck, bool inDoubleCheck) const noexcept;
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept;
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept;
    bool isCastlePathSafe(uint64_t squaresMask, uint8_t byColor) const noexcept;
    bool inCheck(uint8_t color) const noexcept;
    bool isDoubleCheck(uint8_t color) const noexcept;
    bool hasAnyLegalMove(uint8_t color) const noexcept;
    bool isStalemate(uint8_t color) const noexcept;
    bool isFiftyMoveRule() const noexcept;
    bool isDraw(uint8_t color) const noexcept;
    bool isThreefoldRepetition() const noexcept;
    bool isTwofoldRepetition() const noexcept;
    void fromFenToBoard(const std::string& fen);
    std::string fromBoardToFen() const;
    Coords getEnPassant() const noexcept;
    constexpr uint64_t getHash() const noexcept { return currentHash; }

    // Set the en-passant square. Used by NMP for save/restore.
    void setEnPassant(const Coords& ep) noexcept {
        enPassant = ep;
    }

    // Restore clocks after null move (setPrevTurn corrupts them)
    void restoreClocks(uint16_t hmc, uint16_t fmc) noexcept {
        halfMoveClock = hmc;
        fullMoveClock = fmc;
    }
    // Methods end

    // Variables start
    static constexpr uint8_t MASK_PIECE = 0x0F;      // 0000 1111
    static constexpr uint8_t MASK_COLOR = 0x08;      // 0000 1000
    static constexpr uint8_t MASK_PIECE_TYPE = 0x07; // 0000 0111
    static constexpr uint8_t WHITE_KING_START  = 60;  // e1
    static constexpr uint8_t BLACK_KING_START  = 4;   // e8
    static constexpr uint8_t WHITE_ROOK_A_START = 56;  // a1
    static constexpr uint8_t WHITE_ROOK_H_START = 63;  // h1
    static constexpr uint8_t BLACK_ROOK_A_START = 0;   // a8
    static constexpr uint8_t BLACK_ROOK_H_START = 7;   // h8
    static constexpr uint8_t CASTLING_RIGHTS_ALL = 0x0F; // All 4 castling rights

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

    static constexpr std::array<char, 8> PIECE_TYPE_TO_CHAR = {
        '.', 'P', 'N', 'B', 'R', 'Q', 'K', '?'
    };
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

    static constexpr std::array<uint64_t, 64> BIT_MASKS = []() constexpr {
        std::array<uint64_t, 64> masks{};
        for (int i = 0; i < 64; ++i) masks[i] = (1ULL << i);
        return masks;
    }();


    std::array<uint64_t, 2> pawns_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> knights_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> bishops_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> rooks_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> queens_bb  = {0ULL, 0ULL};
    std::array<uint64_t, 2> kings_bb   = {0ULL, 0ULL};
    // Variables end
private:
    // Methods start
    [[nodiscard]] static inline bool isSimplePieceLegal(uint64_t bitMap, uint64_t toBit) noexcept;
    [[nodiscard]] inline bool isKingSafeAfterMove(
        uint8_t movingColor,
        uint8_t fromIndex,
        uint8_t toIndex,
        uint64_t capturedEnemyMask
    ) const noexcept;
    [[nodiscard]] inline bool isKingSafeAfterEnPassant(
        uint8_t movingColor,
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t capturedPawnIndex
    ) const noexcept;
    [[nodiscard]] inline bool isKingMoveLegal(
        uint8_t fromIndex,
        uint8_t toIndex,
        uint64_t toBit,
        uint8_t movingColor
    ) const noexcept;
    [[nodiscard]] inline bool canCastleToSquare(uint8_t fromIndex, uint8_t toIndex, uint8_t movingColor) const noexcept;
    [[nodiscard]] inline bool canCastleGeneric(bool isWhite, uint8_t fromIndex, bool isKingside) const noexcept;
    [[nodiscard]] inline bool verifyKingSafetyForSimplePiece(
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t movingColor,
        uint8_t destPiece
    ) const noexcept;
    [[nodiscard]] static inline bool hasAtLeastTwoBits(uint64_t bb) noexcept;
    [[nodiscard]] static inline bool addAttackAndDetectDouble(uint64_t attackSet, uint8_t& attackers) noexcept;
    [[nodiscard]] static inline uint8_t rookStartSlot(uint8_t index) noexcept;
    inline void clearCastlingByRookStart(uint8_t rookStartIndex, bool setHasMovedBit) noexcept;
    inline void updateCastlingRightsOnPieceMove(uint8_t movingType, uint8_t movingColor, uint8_t fromIndex) noexcept;
    inline void updateCastlingRightsOnRookCapture(uint8_t capturedPiece, uint8_t toIndex) noexcept;
    template<uint8_t PieceType, bool Add>
    inline void updatePieceTypeBB(uint8_t color, uint64_t bit) noexcept;
    template<bool Add>
    inline void dispatchPieceBBUpdate(uint8_t pieceType, uint8_t color, uint64_t bit) noexcept;
    [[nodiscard]] static constexpr bool isCaptureKind(MoveKind kind) noexcept;
    [[nodiscard]] static constexpr bool isPromotionKind(MoveKind kind) noexcept;
    [[nodiscard]] static inline MoveKind classifyMoveKind(
        uint8_t movingType,
        uint8_t movingColor,
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t destBefore,
        const Coords& prevEnPassant
    ) noexcept;
    [[nodiscard]] static inline uint8_t normalizePromotionChoice(char promotionChoice) noexcept;
    [[nodiscard]] static inline uint8_t promotedPieceFromChoice(uint8_t promo, uint8_t movingColor) noexcept;
    inline void snapshotState(MoveState& st) const noexcept;
    inline void restoreState(const MoveState& st) noexcept;
    template<MoveKind Kind>
    inline void doMoveByKind(
        const Coords& from,
        const Coords& to,
        MoveState& st,
        uint8_t moving,
        uint8_t movingType,
        uint8_t movingColor,
        uint8_t destBefore,
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t fromFile,
        uint8_t fromRank,
        uint8_t toFile,
        uint8_t toRank,
        char promotionChoice
    ) noexcept;
    template<MoveKind Kind>
    inline void undoMoveByKind(
        const Coords& from,
        const Coords& to,
        const MoveState& st,
        uint8_t& pieceOnTo,
        uint8_t fromIndex,
        uint8_t toIndex
    ) noexcept;

    bool isKingAttackedCustom(uint8_t kingSq, uint8_t bySide, uint64_t occ,
                              uint64_t pawns, uint64_t knights, uint64_t bishops,
                              uint64_t rooks, uint64_t queens, uint64_t kings) const noexcept;
    bool isSquareAttackedWithOcc(uint8_t targetIndex, uint8_t byColor, uint64_t occ) const noexcept;
    uint8_t charToPiece(char symbol);
    bool parseBoardSection(const std::string& boardSection, std::array<uint32_t, 8>& parsedBoard);
    uint8_t parseActiveColor(const std::string& activeSection);
    std::vector<bool> parseCastling(const std::string& castlingSection);
    Coords parseEnPassant(const std::string& enPassantSection);
    uint8_t safeParseInt(const std::string& section, int min, int max, int defaultValue);
    std::string boardToFenPieces() const;
    char pieceTypeToChar(uint8_t pieceType) const;
    std::string castlingToFen() const;
    std::string enPassantToFen() const;
    void rebuildRepetitionHistory() noexcept;
    void updateRepetitionAfterMove(bool resetHistory, bool recomputeHash = true) noexcept;
    // Methods end
    
    // Variables start

    board chessboard; // 8 * 32 bit = 256 bit = 32 byte

    uint8_t  castle = CASTLING_RIGHTS_ALL;  // Castling rights (KQkq) - 4 bits
    uint8_t  hasMoved = 0x00;               // Piece movement tracking - 6 bits
    Coords   enPassant{};                   // En-passant target square
    uint16_t halfMoveClock = 0;             // 50-move rule counter
    uint16_t fullMoveClock = 1;             // Current move number
    uint8_t  activeColor = WHITE;           // Current side to move
    uint64_t currentHash = 0ULL;            // Zobrist hash of current position
    std::array<uint64_t, 128> repetitionHistory{}; // Ring buffer of recent positions (bounded by 50-move rule)
    uint8_t  historySize = 0;               // Entries valid in repetitionHistory
    std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; 
    uint64_t occupancy = 0ULL;              // Combined occupancy bitboard
    // Variables end
}; // Class Board

#include "board.inl"
#include "boardapi.inl"

} // namespace chess

#endif
