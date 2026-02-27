#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <array>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <string>
#include <cstring>
#include <atomic>
#include <thread>
#include <omp.h>

#ifdef DEBUG
#include <chrono>
#endif

#include "../printer/menu.hpp"
#include "../printer/prints.hpp"
#include "../board/board.hpp"
#include "../board/coords.hpp"

#include "basebonuspenaltyvalues.hpp"
#include "piecevaluetables.hpp"
#include "inl/bitboard_helpers.inl"
#include "../tt/tt.hpp"
#include "movelist.hpp"

namespace engine {

// ===================================================
// BITBOARD HELPERS
// ===================================================

class Engine final {
public:
    // Structs and enums
    struct ScoredMove {
        chess::Board::Move move;
        int64_t score;
    };

    enum GameResult : uint8_t {
        ONGOING = 0,
        WHITE_WINS = 1,
        BLACK_WINS = 2,
        DRAW = 3
    };
    //--- Structs and enums end

    //--- Constructors
    Engine();
    explicit Engine(const std::string& fen);
    ~Engine() noexcept;
    
    // Engine is non-copyable and non-movable due to complex state
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;
    //--- Constructors end

    //--- Method
    //static constexpr int manhattan(int a, int b) noexcept;
    static int64_t evaluateCheckmate(const chess::Board& board) noexcept;

    void reset() noexcept;
    bool movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece = '\0') noexcept;

    void search(uint64_t depth) noexcept;
    void stopThinking() noexcept;
    void setPonderDebugEnabled(bool enabled) noexcept;
    bool isPonderDebugEnabled() const noexcept;
    uint64_t getPonderCurrentDepth() const noexcept;
    uint64_t getPonderLastCompletedDepth() const noexcept;
    uint64_t getPonderInterruptedDepth() const noexcept;
    int64_t evaluate(const chess::Board& board) noexcept; 
    
    bool isGameOver() const noexcept;
    bool isMate() const noexcept;
    bool isStalemate() const noexcept;
    void updateGameResult() noexcept;
    GameResult getGameResult() const noexcept;
    uint8_t getActiveColor() const noexcept;
    
    static constexpr uint64_t adjacentFilesMask(int file) noexcept;
    static constexpr std::array<uint64_t, 8> initFileMasks() noexcept;
    static constexpr std::array<uint64_t, 8> initAdjacentFilesOnly() noexcept;
    static constexpr std::array<uint64_t, 8> initAdjacentAndFileMasks() noexcept;
    static constexpr std::array<uint64_t, 64> initKingProximityMasks() noexcept;
    static constexpr std::array<uint64_t, 64> initWhiteForwardFill();
    static constexpr std::array<uint64_t, 64> initBlackForwardFill();
    // File masks (already defined in fileMask() but we precalculate for speed)
    static const std::array<uint64_t, 8> FILE_MASKS;
    // Adjacent files ONLY (without center file) - optimization for isolated pawn check
    static const std::array<uint64_t, 8> ADJACENT_FILES_ONLY;
    // Precalculated adjacent files mask (including center file)
    static const std::array<uint64_t, 8> ADJACENT_AND_FILE_MASKS;
    // King proximity masks (squares at distance <= 2 from each square)
    static const std::array<uint64_t, 64> KING_PROXIMITY_MASKS;

    static int64_t getMaterialDelta(const chess::Board& b) noexcept;
    // Exposed for perf tests (delegates to Evaluator)
    static int64_t evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static int64_t evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static int64_t evalEndgameKingActivity(const chess::Board& b) noexcept;
    static int64_t evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;

    // DEBUG: Trace version of evaluate that prints each component
    int64_t evaluateTrace(const chess::Board& board) noexcept;

    // Magic bitboard initialization (shared across all Engine instances)
    static inline bool magicTablesInitialized = false;
    static void ensureMagicTablesInitialized() noexcept;

    // Legal move generation (bitboard-based)
    static MoveList<chess::Board::Move> generateLegalMoves(const chess::Board& b) noexcept;
    MoveList<ScoredMove> sortLegalMoves(const MoveList<chess::Board::Move>& moves, int ply, chess::Board& b, bool usIsWhite, uint64_t hashKey, const chess::Board::Move* previousMove = nullptr) noexcept;

    chess::Board::Move getBestMove(chess::Board& rootBoard, const MoveList<chess::Board::Move>& moves, bool searchBestMoveForWhite) noexcept;
    chess::Board::Move getBestMove(chess::Board& rootBoard, const MoveList<chess::Board::Move>& moves, bool searchBestMoveForWhite, int64_t alpha, int64_t beta) noexcept;
    //--- Method end

    //--- Variables
    chess::Board::Move bestMove;

    // Data members
    chess::Board board;
    bool isPlayerWhite;

    uint64_t depth;
    int64_t eval = 0;

    // Transposition table
    tt::TranspositionTable tt;

    uint64_t nodesSearched = 0; 
    int32_t UCI_DEPTH = 0;
    static constexpr int32_t DEFAULTDEPTH = 10;
    static constexpr int32_t MAX_PLY = 64;
    std::string moveHistory = "";

#ifdef DEBUG
    // Transposition table statistics
    static uint64_t ttProbes;
    static uint64_t ttHits;
#endif

    int MAX_THREADS;
    // Dark/Light square masks for bad bishop evaluation
    static constexpr uint64_t DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
    static constexpr uint64_t LIGHT_SQUARES = ~DARK_SQUARES;
    //--- Variables end

private:
    //--- Structs and enums
    // Helper structures for cleaner function signatures
    struct SearchContext {
        int64_t depth;
        int64_t alpha;
        int64_t beta;
        int ply;
        uint8_t activeColor;
        const chess::Board::Move* previousMove = nullptr; // For counter-move history
        int64_t staticEval = 0;   // Static evaluation for pruning decisions
        bool inCheck = false;      // Whether the side to move is in check
        bool isPVNode = false;     // True for full-window nodes (no null-window pruning)
        uint64_t* nodeCounter = nullptr; // Per-search counter (thread-local ownership)
    };

    struct AlphaBeta {
        int64_t alpha;
        int64_t beta;
    };

    struct IterativeSearchResult {
        bool hasLegalMoves = false;
        bool completedAnyDepth = false;
        uint64_t startDepth = 0;
        uint64_t targetDepth = 0;
        uint64_t completedIterations = 0;
        uint64_t completedDepth = 0;
        uint64_t completedEvenDepth = 0;
        uint64_t interruptedDepth = 0;
        uint32_t aspirationResearches = 0;
        uint32_t aspirationFailLow = 0;
        uint32_t aspirationFailHigh = 0;
        tt::TranspositionTable::Entry::Flag rootScoreBound = tt::TranspositionTable::Entry::EXACT;
        chess::Board::Move bestMove{};
        int64_t bestScore = 0;
    };

    //--- Variables
    GameResult gameResult = Engine::ONGOING;
    // Keep search bounds within int32 range so TT store/probe can preserve mate scores.
    constexpr static int64_t NEG_INF = static_cast<int64_t>(std::numeric_limits<int32_t>::min() + 1);
    constexpr static int64_t POS_INF = static_cast<int64_t>(std::numeric_limits<int32_t>::max() - 1);
    
    // Killer moves: up to 2 non-capture moves per ply that previously caused a beta cutoff
    chess::Board::Move killerMoves[2][MAX_PLY] {};

    // History heuristic: bonus for non-capture moves that often cause cutoffs
    // history[colorIndex][fromIndex][toIndex]
    int32_t history[2][64][64] = {};

    // Counter-move history: tracks best response to opponent's previous move
    // counterMoves[prevFrom][prevTo] -> best response move
    // Improves move ordering in tactical sequences
    chess::Board::Move counterMoves[64][64] {};

    // Capture history: bonus for captures that often cause cutoffs
    // 2 slots per bucket to keep a short "recent + secondary" memory.
    // captureHistory[color][to][victimType][slot]
    static constexpr int CAPTURE_HISTORY_SLOTS = 2;
    int32_t captureHistory[2][64][7][CAPTURE_HISTORY_SLOTS] = {};

    static constexpr int64_t PIECE_VALUES[8] = {
        0,      // EMPTY = 0
        PAWN_VALUE,    // PAWN = 1
        KNIGHT_VALUE,    // KNIGHT = 2
        BISHOP_VALUE,    // BISHOP = 3
        ROOK_VALUE,    // ROOK = 4
        QUEEN_VALUE,    // QUEEN = 5
        KING_VALUE,  // KING = 6
        0       // unused = 7
    };
    //--- Variables end

    // Initial best score for min-max search
    // White maximizes: starts from -INF, Black minimizes: starts from +INF
    template<bool IsWhite>
    static constexpr int64_t initialBest() noexcept;
    // Runtime version (when color is not known at compile time)
    static constexpr int64_t initialBest(bool isWhite) noexcept;
    
    // Compare if newScore is better than currentBest (color-aware)
    // White: newScore > currentBest (maximize)
    // Black: newScore < currentBest (minimize)
    template<bool IsWhite>
    static constexpr bool isBetter(int64_t newScore, int64_t currentBest) noexcept;
    // Runtime version
    static constexpr bool isBetter(int64_t newScore, int64_t currentBest, bool isWhite) noexcept;
    
    // Check if we have a beta cutoff (position too good, opponent won't allow it)
    // White (maximizer): score >= beta
    // Black (minimizer): score <= alpha
    static inline bool isBetaCutoff(int64_t score, int64_t alpha, int64_t beta, bool isWhite) noexcept;
    
    // Update alpha or beta bound based on score
    // White (maximizer): alpha = max(alpha, score)
    // Black (minimizer): beta = min(beta, score)
    static inline void updateBound(int64_t score, int64_t& alpha, int64_t& beta, bool isWhite) noexcept;
    
    // Check delta pruning condition
    // White: standPat + margin < alpha (can't reach alpha even with best capture)
    // Black: standPat - margin > beta (can't reach beta even with best capture)
    static inline bool shouldDeltaPrune(int64_t standPat, int64_t margin, int64_t alpha, int64_t beta, bool isWhite) noexcept;
    
    // Return the cutoff value when beta cutoff occurs
    // White: return beta, Black: return alpha
    static inline int64_t cutoffValue(int64_t alpha, int64_t beta, bool isWhite) noexcept;

    // True when a null-window search failed and needs full-window re-search.
    static inline bool shouldResearchPVS(int64_t score, int64_t alphaBound, int64_t betaBound, bool isWhite) noexcept;

    // Fast access to piece values (inline for zero-cost abstraction)
    static inline constexpr int64_t getPieceValue(uint8_t pieceType) noexcept;

    void updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                 chess::Board::Move& bestMove, const chess::Board::Move& m) noexcept;

    void updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int32_t (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY], const chess::Board::Move* previousMove = nullptr) noexcept;
    static int64_t stalemateScoreFromMaterialDelta(int64_t matDelta) noexcept;

    // Search helpers
    int64_t searchRootMoveScore(chess::Board& b, const chess::Board::Move& m, int64_t alpha, int64_t beta,
                                int currPly, bool useTT, bool allowTTWrite, uint64_t* nodeCounter) noexcept;
    bool handleSearchPrelude(const int64_t& depth, const AlphaBeta& bounds, int64_t& score, uint64_t hashKey) noexcept;
    ScoredMove searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                          bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds, bool allowUpdates, bool allowTTWrite = true) noexcept;
    
    // Move scoring helpers
    static uint8_t getLeastValuableAttackerTo(const chess::Board& b, uint8_t sq, uint64_t occLocal, int sideLocal) noexcept;
    int64_t staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept;
    static inline int64_t scoreMoveOrderingPriorityInline(
        chess::Board& b,
        const chess::Board::Move& m,
        uint8_t fromPieceType,
        bool isCapture,
        uint8_t victimType,
        int64_t see,
        bool isPromotionCandidate,
        int moveIndex,
        bool hashMoveIsLegal,
        uint8_t hashFrom,
        uint8_t hashTo,
        char hashPromo,
        int ply,
        const chess::Board::Move* previousMove,
        int usSide,
        uint8_t oppKingSq,
        uint64_t occ,
        bool usIsWhite,
        bool isEndgameOrdering,
        int fullMoveClock,
        const int32_t (&history)[2][64][64],
        const chess::Board::Move (&killerMoves)[2][64],
        const chess::Board::Move (&counterMoves)[64][64],
        const int32_t (&captureHistory)[2][64][7][2],
        const int64_t (&pieceValues)[8],
        int64_t orderingPenaltySamePawnOpening) noexcept;

    int64_t searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply, bool useTT = true, bool allowTTWrite = true, const chess::Board::Move* previousMove = nullptr, uint64_t* nodeCounter = nullptr, bool allowNullMove = true) noexcept;
    int64_t quiescenceSearch(chess::Board& b, int64_t alpha, int64_t beta, int ply, bool useTT = true, uint64_t* nodeCounter = nullptr) noexcept;
    bool isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const noexcept;
    inline bool shouldAbortSearch() const noexcept;

    IterativeSearchResult runIterativeDeepening(chess::Board& rootBoard, uint64_t startDepth, uint64_t targetDepth, bool allowStop) noexcept;
    void storeRootHashMove(const chess::Board& rootBoard, const chess::Board::Move& move, uint64_t depth, int64_t score, uint8_t flag = tt::TranspositionTable::Entry::EXACT) noexcept;
    void startPondering() noexcept;
    void stopPondering() noexcept;
    void ponderLoop(chess::Board rootBoard) noexcept;
    
    // Quiescence helper: generates only tactical moves (captures, promotions)
    static MoveList<chess::Board::Move> generateTacticalMoves(const chess::Board& b, bool includeChecks = false,
                                                       bool inCheckKnown = false, bool inCheckValue = false,
                                                       bool inDoubleCheckValue = false) noexcept;
    //--- Method end

    std::thread ponderingThread;
    std::atomic<bool> ponderingStopRequested {false};
    std::atomic<bool> ponderingActive {false};
    std::atomic<bool> stopSearchRequested {false};
    std::atomic<bool> searchInterrupted {false};
    std::atomic<bool> ponderDebugEnabled {false};
    std::atomic<uint64_t> ponderCurrentDepth {0};
    std::atomic<uint64_t> ponderLastCompletedDepth {0};
    std::atomic<uint64_t> ponderLastCompletedEvenDepth {0};
    std::atomic<uint64_t> ponderInterruptedDepth {0};
    std::atomic<uint32_t> ponderAspirationResearches {0};
    std::atomic<uint32_t> ponderAspirationFailLow {0};
    std::atomic<uint32_t> ponderAspirationFailHigh {0};
}; //class Engine final

} // namespace engine

#include "inl/bitboard_helpers.inl"
#include "inl/precomputed_masks.inl"
#include "inl/search_bounds.inl"
#include "inl/search_cutoffs.inl"
#include "inl/accessors.inl"
#include "inl/search_helpers.inl"

#endif
