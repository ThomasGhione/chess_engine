#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>
#include <cctype>
#include <sstream>
#include <algorithm>
#include <cstddef>

#include "../engine/eval_constants.hpp"
#include "../engine/piecevaluetables.hpp"
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

    enum piece_id : uint8_t {
    
    // piece bits
    EMPTY  = 0x0, // 0000 
    PAWN   = 0x1, // 0001
    KNIGHT = 0x2, // 0010
    BISHOP = 0x3, // 0011
    ROOK   = 0x4, // 0100
    QUEEN  = 0x5, // 0101
    KING   = 0x6, // 0110
    
    // color bit (bit 3 set => white, clear => black)
    BLACK  = 0x0, // 0000
    WHITE  = 0x8, // 1000

    INVALID_WHITE = 0xF, // 1111
    INVALID_BLACK = 0x7, // 0111
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

    enum EvalCacheTerm : uint32_t {
        EVAL_CACHE_MATERIAL_DELTA           = 0,
        EVAL_CACHE_PAWN_STRUCTURE_MG        = 1,
        EVAL_CACHE_PAWN_STRUCTURE_EG        = 2,
        EVAL_CACHE_BISHOP_PAIR_BONUS        = 3,
        EVAL_CACHE_CASTLING_BONUS           = 4,
        EVAL_CACHE_ROOKS                    = 5,
        EVAL_CACHE_BAD_BISHOP               = 6,
        EVAL_CACHE_BLOCKED_PAWN_BY_BISHOPS  = 7,
        EVAL_CACHE_MINOR_DEVELOPMENT        = 8,
        EVAL_CACHE_EARLY_QUEEN              = 9,
        EVAL_CACHE_OUTPOSTS                 = 10,
        EVAL_CACHE_PIECE_COORDINATION       = 11,
        EVAL_CACHE_CENTRAL_CONTROL          = 12,
        EVAL_CACHE_COUNT                    = 13
    };

    static constexpr uint32_t evalCacheBit(uint32_t term) noexcept { return 1u << term; }

    enum MoveChangeFlag : uint32_t {
        MOVE_CHANGE_NONE        = 0u,
        MOVE_CHANGE_CAPTURE     = 1u << 0,
        MOVE_CHANGE_PROMOTION   = 1u << 1,
        MOVE_CHANGE_PAWN_MOVE   = 1u << 2,
        MOVE_CHANGE_KNIGHT_MOVE = 1u << 3,
        MOVE_CHANGE_BISHOP_MOVE = 1u << 4,
        MOVE_CHANGE_ROOK_MOVE   = 1u << 5,
        MOVE_CHANGE_QUEEN_MOVE  = 1u << 6,
        MOVE_CHANGE_KING_MOVE   = 1u << 7,
        MOVE_CHANGE_CASTLING    = 1u << 8
    };

    struct EvalCache {
        std::array<int32_t, EVAL_CACHE_COUNT> terms{};
        uint32_t validMask = 0;
    };

    struct Move {
        Coords from;
        Coords to;
        char promotionPiece = '\0'; // 'q', 'r', 'b', 'n' or '\0' for no promotion
        
        bool operator==(const Move& other) const noexcept;
        
        // Optimized custom rotate: move moves[index] to moves[0] and shift moves[0..index-1] one position to the right
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
        uint8_t prevEpHashFile{0xFF}; // hashed EP file before move, or 0xFF if none
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
        EvalCache prevEvalCache{};
        uint32_t prevLastMoveChangeFlags{MOVE_CHANGE_NONE};
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
    static constexpr uint8_t promotionRank(bool isWhite) noexcept;
    static constexpr uint64_t bitMask(uint8_t sq) noexcept;
    static constexpr uint8_t file(uint8_t sq) noexcept;
    static constexpr uint8_t rank(uint8_t sq) noexcept;

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
    inline void updateChessboard(const Coords& from, const Coords& to, piece_id piece) noexcept;

    __attribute__((always_inline))
    void fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept;

    __attribute__((always_inline))
    void addPieceToBB(uint8_t piece, uint8_t index) noexcept;

    __attribute__((always_inline))
    void removePieceFromBB(uint8_t piece, uint8_t index) noexcept;

    __attribute__((hot))
    bool isCheckmate(uint8_t color) const noexcept;

    constexpr uint8_t getActiveColor() const noexcept;
    constexpr bool getCastle(uint8_t index) const noexcept;
    constexpr uint16_t getFullMoveClock() const noexcept;
    
    constexpr uint8_t operator[](const Coords& coords) const noexcept;
    uint8_t operator[](const Coords& coords) noexcept;
    constexpr uint8_t operator[](uint8_t index) const noexcept; // assert index 0-63
    uint8_t operator[](uint8_t index) noexcept;
    constexpr bool operator==(const Board& other) const noexcept;
    constexpr bool operator!=(const Board& other) const noexcept;
    
    uint64_t getPiecesBitMap() const noexcept;
    void updateOccupancyBB() noexcept;
    
    void doMove(const Move& m, MoveState& state, char promotionChoice = 'q') noexcept;
    void undoMove(const Move& m, const MoveState& state) noexcept;
    void doNullMove(MoveState& state) noexcept;
    void undoNullMove(const MoveState& state) noexcept;
    
    bool move(const Coords& from, const Coords& to, char promotionChoice = '\0') noexcept;
    bool promote(const Coords& at, char choice) noexcept;
    bool isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, bool inCheck, bool inDoubleCheck = false) const noexcept;
    bool isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, uint8_t fromPiece, bool inCheck, bool inDoubleCheck) const noexcept;
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor) const noexcept;
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare) const noexcept;
    bool isCastlePathSafe(uint64_t squaresMask, uint8_t byColor) const noexcept;
    bool inCheck(uint8_t color) const noexcept;
    bool isDoubleCheck(uint8_t color) const noexcept;
    bool hasAnyLegalMove(uint8_t color) const noexcept;
    [[nodiscard]] inline bool isKingSafeAfterMove(
        uint8_t movingColor,
        uint8_t fromIndex,
        uint8_t toIndex,
        uint64_t capturedEnemyMask
    ) const noexcept;
    bool isStalemate(uint8_t color) const noexcept;
    bool isFiftyMoveRule() const noexcept;
    bool isDraw(uint8_t color) const noexcept;
    bool isThreefoldRepetition() const noexcept;
    bool isTwofoldRepetition() const noexcept;
    void fromFenToBoard(const std::string& fen);
    std::string fromBoardToFen() const;
    Coords getEnPassant() const noexcept;
    constexpr uint64_t getHash() const noexcept { return currentHash; }
    constexpr int32_t getIncrementalMaterialDelta() const noexcept;
    int32_t getIncrementalPsqtDelta(bool isEndgame) const noexcept;
    bool hasEvalCacheTerm(uint32_t term) const noexcept;
    template<uint32_t Term>
    bool hasEvalCacheTerm() const noexcept;
    int32_t getEvalCacheTerm(uint32_t term) const noexcept;
    template<uint32_t Term>
    int32_t getEvalCacheTerm() const noexcept;
    void setEvalCacheTerm(uint32_t term, int32_t value) const noexcept;
    template<uint32_t Term>
    void setEvalCacheTerm(int32_t value) const noexcept;
    void invalidateEvalCacheTerms(uint32_t terms) noexcept;
    void clearEvalCache() noexcept;

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
        
        table['P'] = PAWN | WHITE;   table['p'] = PAWN | BLACK;
        table['N'] = KNIGHT | WHITE; table['n'] = KNIGHT | BLACK;
        table['B'] = BISHOP | WHITE; table['b'] = BISHOP | BLACK;
        table['R'] = ROOK | WHITE;   table['r'] = ROOK | BLACK;
        table['Q'] = QUEEN | WHITE;  table['q'] = QUEEN | BLACK;
        table['K'] = KING | WHITE;   table['k'] = KING | BLACK;
        
        return table;
    }();

    static constexpr std::array<char, 8> PIECE_TYPE_TO_CHAR = {
        '.', 'P', 'N', 'B', 'R', 'Q', 'K', '?'
    };
    static constexpr std::array<int32_t, 8> MATERIAL_VALUES = {
        0,
        engine::PAWN_VALUE,
        engine::KNIGHT_VALUE,
        engine::BISHOP_VALUE,
        engine::ROOK_VALUE,
        engine::QUEEN_VALUE,
        engine::KING_VALUE,
        0
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
    [[nodiscard]] inline bool canCastleToSquare(uint8_t fromIndex, uint8_t movingColor, bool isKingside) const noexcept;
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
    inline void updatePieceTypeBB(uint8_t color, uint64_t bit, uint8_t index) noexcept;
    template<bool Add>
    inline void dispatchPieceBBUpdate(uint8_t pieceType, uint8_t color, uint64_t bit, uint8_t index) noexcept;
    template<uint8_t PieceType, bool Add>
    inline void updateIncrementalEvalForPiece(uint8_t color, uint8_t index) noexcept;
    template<uint32_t Term>
    inline int32_t& evalCacheTermRef() const noexcept;
    [[nodiscard]] static constexpr bool isCaptureKind(MoveKind kind) noexcept;
    [[nodiscard]] static constexpr bool isPromotionKind(MoveKind kind) noexcept;
    [[nodiscard]] static inline uint32_t computeMoveChangeFlags(const MoveState& st) noexcept;
    [[nodiscard]] static inline uint32_t evalInvalidationMaskFromMoveFlags(uint32_t moveFlags) noexcept;
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
    inline void promoteUnchecked(uint8_t atIndex, uint8_t pawnPiece, uint8_t promo) noexcept;
    inline void snapshotState(MoveState& st) const noexcept;
    inline void prepareMoveState(MoveState& st, uint8_t moving, uint8_t destBefore) const noexcept;
    inline void prepareNullMoveState(MoveState& st) const noexcept;
    inline void applyEvalCacheInvalidation(const MoveState& st) noexcept;
    inline void restoreState(const MoveState& st) noexcept;
    template<MoveKind Kind>
    inline void doMoveByKind(
        MoveState& st,
        uint8_t moving,
        uint8_t movingType,
        uint8_t movingColor,
        uint8_t destBefore,
        uint8_t fromIndex,
        uint8_t toIndex,
        char promotionChoice
    ) noexcept;
    template<MoveKind Kind>
    inline void undoMoveByKind(
        const MoveState& st,
        uint8_t& pieceOnTo,
        uint8_t fromIndex,
        uint8_t toIndex
    ) noexcept;

    static bool isKingAttackedCustom(uint8_t kingSq, uint8_t bySide, uint64_t occ,
                              uint64_t pawns, uint64_t knights, uint64_t bishops,
                              uint64_t rooks, uint64_t queens, uint64_t kings) noexcept;
    bool isSquareAttackedWithOcc(uint8_t targetIndex, uint8_t byColor, uint64_t occ) const noexcept;
    static uint8_t charToPiece(char symbol);
    static bool parseBoardSection(const std::string& boardSection, std::array<uint32_t, 8>& parsedBoard);
    static uint8_t parseActiveColor(const std::string& activeSection);
    static Coords parseEnPassant(const std::string& enPassantSection);
    static uint8_t safeParseInt(const std::string& section, int min, int max, int defaultValue);
    std::string boardToFenPieces() const;
    static char pieceTypeToChar(uint8_t pieceType);
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
    uint8_t  epHashFile = 0xFF;             // hashed EP file in currentHash, or 0xFF if not hashed
    uint16_t halfMoveClock = 0;             // 50-move rule counter
    uint16_t fullMoveClock = 1;             // Current move number
    uint8_t  activeColor = WHITE;           // Current side to move
    uint64_t currentHash = 0ULL;            // Zobrist hash of current position
    std::array<uint64_t, 128> repetitionHistory{}; // Ring buffer of recent positions (bounded by 50-move rule)
    uint8_t  historySize = 0;               // Entries valid in repetitionHistory
    static constexpr const char* STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    uint64_t occupancy = 0ULL;              // Combined occupancy bitboard
    int32_t incrementalMaterialDelta = 0;
    int32_t incrementalPsqtPawnsMg = 0;
    int32_t incrementalPsqtPawnsEg = 0;
    int32_t incrementalPsqtPieces = 0;
    int32_t incrementalPsqtKingsMg = 0;
    int32_t incrementalPsqtKingsEg = 0;
    mutable EvalCache evalCache{};
    uint32_t lastMoveChangeFlags = MOVE_CHANGE_NONE;
    // Variables end
}; // Class Board

#include "board.inl"
#include "boardapi.inl"

} // namespace chess

#endif
