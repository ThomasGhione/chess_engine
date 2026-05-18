#pragma once

#include <string>
#include <array>
#include <cstdint>
#include <cctype>
#include <cstddef>
#include <cstring>

#include "../engine/eval_constants.hpp"
#include "../engine/piecevaluetables.hpp"
#include "./coords.hpp"
#include "./piece.hpp" // bitmap utilities

namespace chess {

using board = std::array<uint32_t, 8>;

class Board {
public:

    //FIXME Spostare enum in classi enum
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
        EVAL_CACHE_WEAK_SQUARES             = 13,
        EVAL_CACHE_BISHOP_VS_KNIGHT         = 14,
        EVAL_CACHE_COUNT                    = 15
    };

    static constexpr uint32_t evalCacheBit(uint32_t term) noexcept { return 1u << term; }

    enum MoveChangeFlag : uint16_t {
        MOVE_CHANGE_NONE        = 0u,
        MOVE_CHANGE_CAPTURE     = 1u << 0,
        MOVE_CHANGE_PROMOTION   = 1u << 1,
        MOVE_CHANGE_PAWN_MOVE   = 1u << 2,
        MOVE_CHANGE_KNIGHT_MOVE = 1u << 3,
        MOVE_CHANGE_BISHOP_MOVE = 1u << 4,
        MOVE_CHANGE_ROOK_MOVE   = 1u << 5,
        MOVE_CHANGE_QUEEN_MOVE  = 1u << 6,
        MOVE_CHANGE_KING_MOVE   = 1u << 7,
        MOVE_CHANGE_CASTLING    = 1u << 8,
        MOVE_CHANGE_ALL         = MOVE_CHANGE_CAPTURE
                                | MOVE_CHANGE_PROMOTION
                                | MOVE_CHANGE_PAWN_MOVE
                                | MOVE_CHANGE_KNIGHT_MOVE
                                | MOVE_CHANGE_BISHOP_MOVE
                                | MOVE_CHANGE_ROOK_MOVE
                                | MOVE_CHANGE_QUEEN_MOVE
                                | MOVE_CHANGE_KING_MOVE
                                | MOVE_CHANGE_CASTLING
    };

    //FIXME Spostare struct in file appositi
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
        uint64_t prevHistoryHead{};
        EvalCache prevEvalCache{};
        uint16_t prevLastMoveChangeFlags{MOVE_CHANGE_NONE};

        uint8_t  prevHalfMoveClock{};
        uint8_t  prevFullMoveClock{};
        uint8_t  prevHistorySize{};

        // En passant and castling rights
        Coords  prevEnPassant{};
        uint8_t prevEpHashFile{0xFF}; // hashed EP file before move, or 0xFF if none
        uint8_t prevCastle{};   // bitmask copy of castle
        uint8_t prevHasMoved{}; // bitmask copy of hasMoved

        // Piece information related to the move
        uint8_t capturedPiece{};        // piece captured on destination or via en-passant (0 if none)
        uint8_t fromPiece{};            // original piece on "from" before the move (useful for promotions)
        uint8_t promotionPieceType{};   // non-zero if the move is a promotion (PAWN=0 -> no promotion)

        uint8_t enPassantCapturedIndex{}; // index (0..63) of the pawn captured en-passant

        uint8_t rookFromIndex{};        // rook start square index in castling
        uint8_t rookToIndex{};          // rook destination square index in castling
        MoveKind moveKind{MoveKind::Quiet};
    };

    static_assert(sizeof(MoveState) <= 96, "MoveState layout regressed; keep it compact for search stack usage.");
    // Structs and enums end
    
    // Constructors start
    Board() noexcept;
    explicit Board(const std::string& fen);
    Board(const Board& other) noexcept;
    Board& operator=(const Board& other) noexcept;
    Board(Board&& other) noexcept;
    Board& operator=(Board&& other) noexcept;
    // Constructors end

    // Methods start
    //FIXME Controllare di non star esponendo metodi pubblici non chiamati esternamente, se possiamo renderli privati e' meglio
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
    constexpr inline uint8_t getColor(uint8_t index) const noexcept;
    
    __attribute__((hot, always_inline))
    inline void set(uint8_t index, piece_id value) noexcept;

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
    
    uint64_t getPiecesBitMap() const noexcept;
    void updateOccupancyBB() noexcept;
    
    void doMove(const Move& m, MoveState& state, char promotionChoice = 'q') noexcept;
    void undoMove(const Move& m, const MoveState& state) noexcept;
    void doNullMove(MoveState& state) noexcept;
    void undoNullMove(const MoveState& state) noexcept;
    
    //FIXME Abbiamo la struttura dati Move, usiamola invece di passare parametri separati
    bool move(const Coords& from, const Coords& to, char promotionChoice = '\0') noexcept;
    bool isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, uint8_t fromPiece) const noexcept;
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare = 64) const noexcept;
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
    int  countRepetitions() const noexcept;
    bool hasInsufficientMaterialDraw() const noexcept;
    void fromFenToBoard(const std::string& fen);
    std::string fromBoardToFen() const;
    Coords getEnPassant() const noexcept;
    constexpr uint64_t getHash() const noexcept { return currentHash; }
    constexpr int32_t getIncrementalMaterialDelta() const noexcept;
    constexpr int32_t getIncrementalNonPawnMajorCount() const noexcept;
    int32_t getIncrementalPsqtDelta(bool isEndgame) const noexcept;
    template<uint32_t Term>
    bool hasEvalCacheTerm() const noexcept;
    template<uint32_t Term>
    int32_t getEvalCacheTerm() const noexcept;
    template<uint32_t Term>
    void setEvalCacheTerm(int32_t value) const noexcept;
    void invalidateEvalCacheTerms(uint32_t terms) noexcept;
    void clearEvalCache() noexcept;

    // Methods end

    // Variables start
    static constexpr uint8_t MASK_PIECE = 0x0F;      // 0000 1111
    static constexpr uint8_t MASK_COLOR = 0x08;      // 0000 1000
    static constexpr uint8_t MASK_PIECE_TYPE = 0x07; // 0000 0111
    static constexpr uint8_t WHITE_ROOK_A_START = 56;  // a1
    static constexpr uint8_t WHITE_ROOK_H_START = 63;  // h1
    static constexpr uint8_t BLACK_ROOK_A_START = 0;   // a8
    static constexpr uint8_t BLACK_ROOK_H_START = 7;   // h8
    static constexpr uint8_t CASTLING_RIGHTS_ALL = 0x0F; // All 4 castling rights
    // 50-move rule bounds reversible plies to 100; +1 keeps current position.
    static constexpr uint16_t REPETITION_HISTORY_CAPACITY = 101;

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

    inline static std::array<int32_t, 8> MATERIAL_VALUES = {
        0,
        engine::PAWN_VALUE,
        engine::KNIGHT_VALUE,
        engine::BISHOP_VALUE,
        engine::ROOK_VALUE,
        engine::QUEEN_VALUE,
        engine::KING_VALUE,
        0
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
    //FIXME Stessa questione citata prima per sostituire con struct Move
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
    [[nodiscard]] inline bool canCastleGeneric(bool isWhite, uint8_t fromIndex, bool isKingside) const noexcept;
    [[nodiscard]] inline bool verifyKingSafetyForSimplePiece(
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t movingColor,
        uint8_t destPiece
    ) const noexcept;
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
    [[nodiscard]] static inline uint16_t computeMoveChangeFlags(const MoveState& st) noexcept;
    template<uint16_t MoveFlags>
    [[nodiscard]] static constexpr uint32_t evalInvalidationMaskFromMoveFlagsConstexpr() noexcept;
    template<uint16_t... MoveFlags>
    [[nodiscard]] static constexpr std::array<uint32_t, sizeof...(MoveFlags)>
    buildEvalInvalidationMaskLut(std::integer_sequence<uint16_t, MoveFlags...>) noexcept;
    [[nodiscard]] static inline uint32_t evalInvalidationMaskFromMoveFlags(uint32_t moveFlags) noexcept;
    [[nodiscard]] static inline MoveKind classifyMoveKind(
        uint8_t movingType,
        uint8_t movingColor,
        uint8_t fromIndex,
        uint8_t toIndex,
        uint8_t destBefore,
        const Coords& prevEnPassant
    ) noexcept;
    [[nodiscard]] static inline uint8_t normalizePromotionChoice(char choice) noexcept;
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
    static bool parseBoardSection(const std::string& boardSection, std::array<uint32_t, 8>& parsedBoard);
    static uint8_t parseActiveColor(const std::string& activeSection);
    static Coords parseEnPassant(const std::string& enPassantSection);
    static uint8_t safeParseInt(const std::string& section, int min, int max, int defaultValue);
    std::string boardToFenPieces() const;
    std::string castlingToFen() const;
    std::string enPassantToFen() const;
    inline void copyFromBoard(const Board& other) noexcept;
    void rebuildRepetitionHistory() noexcept;
    void updateRepetitionAfterMove(bool resetHistory, bool recomputeHash = true) noexcept;
    // Methods end
    
    // Variables start

    //FIXME Dentro board abbiamo board?
    board chessboard; // 8 * 32 bit = 256 bit = 32 byte
    uint64_t currentHash = 0ULL;            // Zobrist hash of current position
    std::array<uint64_t, REPETITION_HISTORY_CAPACITY> repetitionHistory{}; // Recent reversible-position hashes
    uint64_t occupancy = 0ULL;              // Combined occupancy bitboard
    
    //FIXME trovare nome piu' significativo per queste varibili
    int32_t incrementalMaterialDelta = 0;
    int32_t incrementalNonPawnMajorCount = 0;
    int32_t incrementalPsqtPawnsMg = 0;
    int32_t incrementalPsqtPawnsEg = 0;
    int32_t incrementalPsqtPieces = 0;
    int32_t incrementalPsqtKingsMg = 0;
    int32_t incrementalPsqtKingsEg = 0;

    mutable EvalCache evalCache{};
    uint16_t lastMoveChangeFlags = MOVE_CHANGE_NONE;
    uint8_t halfMoveClock = 0;             // 50-move rule counter
    uint8_t fullMoveClock = 1;             // Current move number
    uint8_t  castle = CASTLING_RIGHTS_ALL;  // Castling rights (KQkq) - 4 bits
    uint8_t  hasMoved = 0x00;               // Piece movement tracking - 6 bits
    Coords   enPassant{};                   // En-passant target square
    uint8_t  epHashFile = 0xFF;             // hashed EP file in currentHash, or 0xFF if not hashed
    uint8_t  activeColor = WHITE;           // Current side to move
    uint8_t  historySize = 0;               // Entries valid in repetitionHistory
    static constexpr const char* STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    // Variables end
}; // Class Board

#include "board.inl"
#include "boardapi.inl"

} // namespace chess
