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
#include "./piece.hpp"

namespace chess {

using board = std::array<uint32_t, 8>;

class Board {
public:
    // --- Enums ---

    //FIXME Spostare enum in classi enum
    enum CastlingBits : uint8_t {
        WHITE_KINGSIDE  = 0,
        WHITE_QUEENSIDE = 1,
        BLACK_KINGSIDE  = 2,
        BLACK_QUEENSIDE = 3
    };

    enum piece_id : uint8_t {
        EMPTY  = 0x0,
        PAWN   = 0x1,
        KNIGHT = 0x2,
        BISHOP = 0x3,
        ROOK   = 0x4,
        QUEEN  = 0x5,
        KING   = 0x6,
        BLACK  = 0x0,
        WHITE  = 0x8,
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
        EVAL_CACHE_MATERIAL_DELTA          = 0,
        EVAL_CACHE_PAWN_STRUCTURE_MG       = 1,
        EVAL_CACHE_PAWN_STRUCTURE_EG       = 2,
        EVAL_CACHE_BISHOP_PAIR_BONUS       = 3,
        EVAL_CACHE_CASTLING_BONUS          = 4,
        EVAL_CACHE_ROOKS                   = 5,
        EVAL_CACHE_BAD_BISHOP              = 6,
        EVAL_CACHE_BLOCKED_PAWN_BY_BISHOPS = 7,
        EVAL_CACHE_MINOR_DEVELOPMENT       = 8,
        EVAL_CACHE_EARLY_QUEEN             = 9,
        EVAL_CACHE_OUTPOSTS                = 10,
        EVAL_CACHE_PIECE_COORDINATION      = 11,
        EVAL_CACHE_CENTRAL_CONTROL         = 12,
        EVAL_CACHE_WEAK_SQUARES            = 13,
        EVAL_CACHE_BISHOP_VS_KNIGHT        = 14,
        EVAL_CACHE_COUNT                   = 15
    };

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
        MOVE_CHANGE_ALL         = MOVE_CHANGE_CAPTURE | MOVE_CHANGE_PROMOTION
                                | MOVE_CHANGE_PAWN_MOVE | MOVE_CHANGE_KNIGHT_MOVE
                                | MOVE_CHANGE_BISHOP_MOVE | MOVE_CHANGE_ROOK_MOVE
                                | MOVE_CHANGE_QUEEN_MOVE | MOVE_CHANGE_KING_MOVE
                                | MOVE_CHANGE_CASTLING
    };

    // --- Public structs ---

    //FIXME Spostare struct in file appositi
    struct EvalCache {
        std::array<int32_t, EVAL_CACHE_COUNT> terms{};
        uint32_t validMask = 0;
    };

    struct Move {
        Coords from;
        Coords to;
        char promotionPiece = '\0';

        bool operator==(const Move& other) const noexcept;

        template<typename MoveContainer>
        static void rotate(MoveContainer& moves, size_t index) noexcept;

        std::string toUCIString() const noexcept;
    };

    struct MoveState {
        uint64_t prevHistoryHead{};
        EvalCache prevEvalCache{};
        uint16_t prevLastMoveChangeFlags{MOVE_CHANGE_NONE};

        uint8_t prevHalfMoveClock{};
        uint8_t prevFullMoveClock{};
        uint8_t prevHistorySize{};

        Coords  prevEnPassant{};
        uint8_t prevEpHashFile{0xFF};
        uint8_t prevCastle{};
        uint8_t prevHasMoved{};

        uint8_t capturedPiece{};
        uint8_t fromPiece{};
        uint8_t promotionPieceType{};
        uint8_t enPassantCapturedIndex{};
        uint8_t rookFromIndex{};
        uint8_t rookToIndex{};
        MoveKind moveKind{MoveKind::Quiet};
    };

    static_assert(sizeof(MoveState) <= 96, "MoveState layout regressed; keep it compact for search stack usage.");

    // --- Static constants ---
    static constexpr uint8_t  MASK_PIECE              = 0x0F;
    static constexpr uint8_t  MASK_COLOR              = 0x08;
    static constexpr uint8_t  MASK_PIECE_TYPE         = 0x07;
    static constexpr uint8_t  WHITE_ROOK_A_START      = 56;
    static constexpr uint8_t  WHITE_ROOK_H_START      = 63;
    static constexpr uint8_t  BLACK_ROOK_A_START      = 0;
    static constexpr uint8_t  BLACK_ROOK_H_START      = 7;
    static constexpr uint8_t  CASTLING_RIGHTS_ALL     = 0x0F;
    // 50-move rule bounds reversible plies to 100; +1 keeps the current position.
    static constexpr uint16_t REPETITION_HISTORY_CAPACITY = 101;

    static constexpr uint32_t evalCacheBit(uint32_t term) noexcept { return 1u << term; }

    static constexpr std::array<uint64_t, 64> BIT_MASKS = []() constexpr {
        std::array<uint64_t, 64> masks{};
        for (int i = 0; i < 64; ++i) masks[i] = (1ULL << i);
        return masks;
    }();

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
        engine::PAWN_VALUE, engine::KNIGHT_VALUE, engine::BISHOP_VALUE,
        engine::ROOK_VALUE, engine::QUEEN_VALUE,  engine::KING_VALUE,
        0
    };

    // --- Constructors ---
    Board() noexcept;
    explicit Board(const std::string& fen);
    Board(const Board& other) noexcept;
    Board& operator=(const Board& other) noexcept;
    Board(Board&& other) noexcept;
    Board& operator=(Board&& other) noexcept;

    // --- Static utilities ---
    static constexpr uint8_t  oppositeColor(uint8_t color) noexcept;
    static constexpr uint8_t  colorToIndex(uint8_t color) noexcept;
    static constexpr uint8_t  promotionRank(bool isWhite) noexcept;
    static constexpr uint64_t bitMask(uint8_t sq) noexcept;
    static constexpr uint8_t  file(uint8_t sq) noexcept;
    static constexpr uint8_t  rank(uint8_t sq) noexcept;

    // --- Board access ---
    __attribute__((hot, always_inline)) constexpr inline uint8_t get(uint8_t index) const noexcept;
    __attribute__((always_inline))      constexpr inline uint8_t get(Coords coords) const noexcept;
    __attribute__((always_inline))      constexpr inline uint8_t get(uint8_t row, uint8_t col) const noexcept;
    __attribute__((always_inline))      constexpr inline uint8_t getColor(uint8_t index) const noexcept;
    __attribute__((hot, always_inline)) inline void set(uint8_t index, piece_id value) noexcept;

    __attribute__((always_inline)) void fastUpdateOccupancyBB(uint8_t fromIndex, uint8_t toIndex) noexcept;
    __attribute__((always_inline)) void addPieceToBB(uint8_t piece, uint8_t index) noexcept;
    __attribute__((always_inline)) void removePieceFromBB(uint8_t piece, uint8_t index) noexcept;

    // --- Move execution ---
    void doMove(const Move& m, MoveState& state, char promotionChoice = 'q') noexcept;
    void undoMove(const Move& m, const MoveState& state) noexcept;
    void doNullMove(MoveState& state) noexcept;
    void undoNullMove(const MoveState& state) noexcept;
    //FIXME Abbiamo la struttura dati Move, usiamola invece di passare parametri separati
    bool move(const Coords& from, const Coords& to, char promotionChoice = '\0') noexcept;

    // --- Legality & attack queries ---
    bool isLegalPseudoMove(uint8_t fromIndex, uint8_t toIndex, uint8_t fromPiece) const noexcept;
    bool isSquareAttacked(uint8_t targetIndex, uint8_t byColor, uint8_t excludeSquare = 64) const noexcept;
    bool inCheck(uint8_t color) const noexcept;
    bool isDoubleCheck(uint8_t color) const noexcept;
    [[nodiscard]] inline bool isKingSafeAfterMove(uint8_t movingColor, uint8_t fromIndex,
                                                   uint8_t toIndex, uint64_t capturedMask) const noexcept;

    // --- Game state queries ---
    __attribute__((hot)) bool isCheckmate(uint8_t color) const noexcept;
    bool hasAnyLegalMove(uint8_t color) const noexcept;
    bool isStalemate(uint8_t color) const noexcept;
    bool isFiftyMoveRule() const noexcept;
    bool isDraw(uint8_t color) const noexcept;
    bool isThreefoldRepetition() const noexcept;
    int  countRepetitions() const noexcept;
    bool hasInsufficientMaterialDraw() const noexcept;

    // --- State accessors ---
    constexpr uint8_t  getActiveColor() const noexcept;
    constexpr bool     getCastle(uint8_t index) const noexcept;
    constexpr uint16_t getFullMoveClock() const noexcept;
    Coords             getEnPassant() const noexcept;
    constexpr uint64_t getHash() const noexcept { return currentHash; }
    uint64_t           getPiecesBitMap() const noexcept;
    void               updateOccupancyBB() noexcept;

    // --- Incremental eval accessors ---
    constexpr int32_t getIncrementalMaterialDelta() const noexcept;
    constexpr int32_t getIncrementalNonPawnMajorCount() const noexcept;
    int32_t           getIncrementalPsqtDelta(bool isEndgame) const noexcept;

    // --- Eval cache ---
    template<uint32_t Term> bool     hasEvalCacheTerm() const noexcept;
    template<uint32_t Term> int32_t  getEvalCacheTerm() const noexcept;
    template<uint32_t Term> void     setEvalCacheTerm(int32_t value) const noexcept;
    void invalidateEvalCacheTerms(uint32_t terms) noexcept;
    void clearEvalCache() noexcept;

    // --- FEN ---
    void        fromFenToBoard(const std::string& fen);
    std::string fromBoardToFen() const;

    // --- Public bitboards (direct access for eval/search hot paths) ---
    std::array<uint64_t, 2> pawns_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> knights_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> bishops_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> rooks_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> queens_bb  = {0ULL, 0ULL};
    std::array<uint64_t, 2> kings_bb   = {0ULL, 0ULL};

private:
    // --- Private helpers: move execution ---
    inline void snapshotState(MoveState& st) const noexcept;
    inline void prepareMoveState(MoveState& st, uint8_t moving, uint8_t destBefore) const noexcept;
    inline void prepareNullMoveState(MoveState& st) const noexcept;
    inline void applyEvalCacheInvalidation(const MoveState& st) noexcept;
    inline void restoreState(const MoveState& st) noexcept;
    template<MoveKind Kind>
    inline void doMoveByKind(MoveState& st, uint8_t moving, uint8_t movingType,
                              uint8_t movingColor, uint8_t destBefore,
                              uint8_t fromIndex, uint8_t toIndex, char promotionChoice) noexcept;
    template<MoveKind Kind>
    inline void undoMoveByKind(const MoveState& st, uint8_t& pieceOnTo,
                                uint8_t fromIndex, uint8_t toIndex) noexcept;
    inline void promoteUnchecked(uint8_t atIndex, uint8_t pawnPiece, uint8_t promo) noexcept;

    // --- Private helpers: castling ---
    [[nodiscard]] inline bool canCastleGeneric(bool isWhite, uint8_t fromIndex, bool isKingside) const noexcept;
    [[nodiscard]] static inline uint8_t rookStartSlot(uint8_t index) noexcept;
    inline void clearCastlingByRookStart(uint8_t rookStartIndex, bool setHasMovedBit) noexcept;
    inline void updateCastlingRightsOnPieceMove(uint8_t movingType, uint8_t movingColor, uint8_t fromIndex) noexcept;
    inline void updateCastlingRightsOnRookCapture(uint8_t capturedPiece, uint8_t toIndex) noexcept;

    // --- Private helpers: legality ---
    [[nodiscard]] inline bool isKingMoveLegal(uint8_t fromIndex, uint8_t toIndex,
                                               uint64_t toBit, uint8_t movingColor) const noexcept;
    [[nodiscard]] inline bool verifyKingSafetyForSimplePiece(uint8_t fromIndex, uint8_t toIndex,
                                                              uint8_t movingColor, uint8_t destPiece) const noexcept;
    template<uint8_t PieceType>
    [[nodiscard]] bool hasLegalMovesForPieceType(uint64_t pieceBB, uint64_t ownOcc,
                                                  uint64_t enemyOcc, uint8_t movingColor) const noexcept;
    static bool isKingAttackedCustom(uint8_t kingSq, uint8_t bySide, uint64_t occ,
                                      uint64_t pawns, uint64_t knights, uint64_t bishops,
                                      uint64_t rooks, uint64_t queens, uint64_t kings) noexcept;

    // --- Private helpers: bitboards & incremental eval ---
    template<uint8_t PieceType, bool Add>
    inline void updatePieceTypeBB(uint8_t color, uint64_t bit, uint8_t index) noexcept;
    template<bool Add>
    inline void dispatchPieceBBUpdate(uint8_t pieceType, uint8_t color, uint64_t bit, uint8_t index) noexcept;
    template<uint8_t PieceType, bool Add>
    inline void updateIncrementalEvalForPiece(uint8_t color, uint8_t index) noexcept;

    // --- Private helpers: eval cache ---
    template<uint32_t Term> inline int32_t& evalCacheTermRef() const noexcept;
    [[nodiscard]] static inline uint16_t computeMoveChangeFlags(const MoveState& st) noexcept;
    [[nodiscard]] static inline uint32_t evalInvalidationMaskFromMoveFlags(uint32_t moveFlags) noexcept;
    template<uint16_t MoveFlags>
    [[nodiscard]] static constexpr uint32_t evalInvalidationMaskFromMoveFlagsConstexpr() noexcept;
    template<uint16_t... MoveFlags>
    [[nodiscard]] static constexpr std::array<uint32_t, sizeof...(MoveFlags)>
    buildEvalInvalidationMaskLut(std::integer_sequence<uint16_t, MoveFlags...>) noexcept;

    // --- Private helpers: move classification ---
    [[nodiscard]] static constexpr bool     isCaptureKind(MoveKind kind) noexcept;
    [[nodiscard]] static constexpr bool     isPromotionKind(MoveKind kind) noexcept;
    [[nodiscard]] static inline MoveKind    classifyMoveKind(uint8_t movingType, uint8_t movingColor,
                                                              uint8_t fromIndex, uint8_t toIndex,
                                                              uint8_t destBefore, const Coords& prevEnPassant) noexcept;
    [[nodiscard]] static inline uint8_t     normalizePromotionChoice(char choice) noexcept;
    [[nodiscard]] static inline uint8_t     promotedPieceFromChoice(uint8_t promo, uint8_t movingColor) noexcept;

    // --- Private helpers: FEN & hash ---
    static bool    parseBoardSection(const std::string& boardSection, std::array<uint32_t, 8>& parsedBoard);
    static uint8_t parseActiveColor(const std::string& activeSection);
    static Coords  parseEnPassant(const std::string& enPassantSection);
    static uint8_t safeParseInt(const std::string& section, int min, int max, int defaultValue);
    std::string    boardToFenPieces() const;
    std::string    castlingToFen() const;
    std::string    enPassantToFen() const;
    void           recomputeHashAndEp() noexcept;
    void           rebuildRepetitionHistory() noexcept;
    void           updateRepetitionAfterMove(bool resetHistory, bool recomputeHash = true) noexcept;
    inline void    copyFromBoard(const Board& other) noexcept;

    // --- Private data ---
    //FIXME Dentro board abbiamo board?
    board    chessboard;
    uint64_t currentHash = 0ULL;
    std::array<uint64_t, REPETITION_HISTORY_CAPACITY> repetitionHistory{};
    uint64_t occupancy   = 0ULL;

    //FIXME trovare nome piu' significativo per queste variabili
    int32_t incrementalMaterialDelta    = 0;
    int32_t incrementalNonPawnMajorCount = 0;
    int32_t incrementalPsqtPawnsMg      = 0;
    int32_t incrementalPsqtPawnsEg      = 0;
    int32_t incrementalPsqtPieces       = 0;
    int32_t incrementalPsqtKingsMg      = 0;
    int32_t incrementalPsqtKingsEg      = 0;

    mutable EvalCache evalCache{};
    uint16_t lastMoveChangeFlags = MOVE_CHANGE_NONE;
    uint8_t  halfMoveClock       = 0;
    uint8_t  fullMoveClock       = 1;
    uint8_t  castle              = CASTLING_RIGHTS_ALL;
    uint8_t  hasMoved            = 0x00;
    Coords   enPassant{};
    uint8_t  epHashFile          = 0xFF;
    uint8_t  activeColor         = WHITE;
    uint8_t  historySize         = 0;

    static constexpr const char* STARTING_FEN =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
};

#include "board.inl"
#include "boardapi.inl"

} // namespace chess
