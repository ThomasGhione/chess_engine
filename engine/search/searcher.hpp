#ifndef SEARCHER_HPP
#define SEARCHER_HPP

#include <cstdint>
#include <atomic>
#include <array>
#include <limits>
#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../engine.hpp"

namespace engine {

// MoveList is defined in engine.hpp
using ChessMoveList = MoveList<chess::Board::Move>;

// ============================================================================
// SEARCH STATE - Mutable state for search heuristics and control
// ============================================================================
struct SearchState {
    // History heuristic: [color][from][to]
    int16_t history[2][64][64] = {};
    
    // Killer moves: [slot][ply]
    static constexpr int MAX_PLY = 64;
    chess::Board::Move killerMoves[2][MAX_PLY] {};
    
    // Counter-move history: [prevFrom][prevTo] -> encoded move
    uint16_t counterMoves[64][64] {};
    
    // Capture history: [color][to][victimType][slot]
    static constexpr int CAPTURE_HISTORY_SLOTS = 2;
    int16_t captureHistory[2][64][7][CAPTURE_HISTORY_SLOTS] = {};
    
    // Search statistics
    uint64_t nodesSearched = 0;
    std::atomic<bool> interrupted {false};
};

// ============================================================================
// SEARCH CONFIG - Immutable configuration for search behavior
// ============================================================================
struct SearchConfig {
    uint8_t maxDepth = 10;
    int maxThreads = 1;
    
    // Search technique toggles
    bool useTT = true;
    bool useLMR = true;
    bool useNullMove = true;
    bool useFutilityPruning = true;
    bool useLateMoveReductions = true;
    bool useAspirationWindows = true;
    
    // Pruning margins and thresholds (tunable)
    int32_t nullMoveReductionBase = 3;
    int32_t nullMoveReductionDepthDiv = 8;
    int32_t futilityMarginMG = 260;
    int32_t futilityMarginEG = 170;
    
    static SearchConfig standard() noexcept;
    static SearchConfig conservative() noexcept;
    static SearchConfig aggressive() noexcept;
};

// ============================================================================
// SEARCH RESULT - Output of search operation
// ============================================================================
struct SearchResult {
    chess::Board::Move bestMove;
    int32_t score = 0;
    uint64_t nodesSearched = 0;
    uint8_t depth = 0;
    bool interrupted = false;
    
    // Bound type from TT
    enum BoundType : uint8_t {
        EXACT = 0,
        LOWERBOUND = 1,
        UPPERBOUND = 2
    };
    BoundType bound = EXACT;
};

// ============================================================================
// SEARCHER - Core search engine with Builder pattern
// ============================================================================
class Searcher {
public:
    // Forward declare Builder
    class Builder;
    
    // Main API - find best move for a position
    SearchResult findBestMove(const chess::Board& position) noexcept;
    
    // Stop ongoing search
    void stop() noexcept;
    
    // Check if search should abort
    bool shouldAbort() const noexcept;
    
    // Access configuration (read-only)
    const SearchConfig& config() const noexcept { return config_; }
    
    // Access state (for inspection/debugging)
    const SearchState& state() const noexcept { return state_; }
    SearchState& state() noexcept { return state_; }

private:
    // Private constructor - use Builder to create
    Searcher(SearchState& state, const SearchConfig& config, tt::TranspositionTable& tt) noexcept;
    
    SearchState& state_;
    SearchConfig config_;
    tt::TranspositionTable& tt_;
    
    // Search bounds helper
    struct AlphaBeta {
        int32_t alpha;
        int32_t beta;
    };
    
    // Search context - groups related parameters
    struct SearchContext {
        int32_t depth;
        int32_t alpha;
        int32_t beta;
        int ply;
        uint8_t activeColor;
        const chess::Board::Move* previousMove = nullptr;
        int32_t staticEval = 0;
        bool inCheck = false;
        bool isPVNode = false;
        uint64_t* nodeCounter = nullptr;
    };
    
    struct SearchNodeState {
        uint8_t activeColor = chess::Board::WHITE;
        bool usIsWhite = true;
        bool inCheck = false;
        bool isPVNode = false;
        bool isPawnEndgameForPruning = false;
        int32_t staticEval = 0;
    };
    
    struct SearchMoveResult {
        chess::Board::Move move;
        int32_t score;
    };
    
    // Core search functions
    int32_t search(chess::Board& b, int32_t depth, int32_t alpha, int32_t beta, int ply,
                   const chess::Board::Move* previousMove = nullptr, 
                   uint64_t* nodeCounter = nullptr, 
                   bool allowNullMove = true) noexcept;
    
    int32_t quiesce(chess::Board& b, int32_t alpha, int32_t beta, int ply,
                    uint64_t* nodeCounter = nullptr) noexcept;
    
    SearchMoveResult searchMoves(chess::Board& b, const ChessMoveList& moves,
                                 bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds) noexcept;
    
    // Iterative deepening
    SearchResult runIterativeDeepening(chess::Board& rootBoard, uint64_t startDepth, uint64_t targetDepth) noexcept;
    
    // Move ordering
    bool sortLegalMoves(ChessMoveList& moves, int ply, chess::Board& b, 
                        bool usIsWhite, uint64_t hashKey, const chess::Board::Move* previousMove) noexcept;
    
    // Pruning techniques
    bool tryNullMovePruning(chess::Board& b, const SearchNodeState& node,
                           int32_t depth, int32_t alpha, int32_t beta, int ply,
                           uint64_t* nodeCounter, int32_t& outScore) noexcept;
    
    bool tryReverseFutilityPruning(chess::Board& b, const SearchNodeState& node,
                                   int32_t depth, int32_t alpha, int32_t beta, int ply,
                                   int32_t& outScore) noexcept;
    
    // Heuristics update
    void updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m,
                                           int32_t depth, int ply, uint8_t us,
                                           const chess::Board::Move* previousMove) noexcept;
    
    // Static Exchange Evaluation
    int32_t staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept;
    
    // Helper functions (previously in anonymous namespace)
    static constexpr int32_t NEG_INF = std::numeric_limits<int32_t>::min();
    static constexpr int32_t POS_INF = std::numeric_limits<int32_t>::max();
    
    static int16_t clampHeuristic16(int32_t value) noexcept;
    static int32_t saturatingAdd32(int32_t lhs, int32_t rhs) noexcept;
    static int32_t saturatingSub32(int32_t lhs, int32_t rhs) noexcept;
    static int32_t stalemateScoreFromMaterialDelta(int32_t matDelta) noexcept;
    static int32_t repetitionDrawScore(const chess::Board& b) noexcept;
    static bool hasInsufficientMaterialDraw(const chess::Board& b) noexcept;
    
    static void doMoveWithPromotion(chess::Board& b, const chess::Board::Move& m, 
                                    chess::Board::MoveState& state) noexcept;
    
    bool isKillerMove(const chess::Board::Move& m, int ply) const noexcept;
    void updateMinMax(bool usIsWhite, int32_t score, int32_t& alpha, int32_t& beta,
                      int32_t& bestScore, chess::Board::Move& bestMove, 
                      const chess::Board::Move& currentMove) noexcept;
    
    friend class Builder;
};

// ============================================================================
// BUILDER - Fluent API for Searcher construction
// ============================================================================
class Searcher::Builder {
public:
    Builder() = default;
    
    Builder& withDepth(uint8_t depth) noexcept;
    Builder& withThreads(int threads) noexcept;
    Builder& withConfig(const SearchConfig& config) noexcept;
    
    // Technique toggles
    Builder& disableTT() noexcept;
    Builder& disableLMR() noexcept;
    Builder& disableNullMove() noexcept;
    Builder& disablePruning() noexcept;
    Builder& disableAspirationWindows() noexcept;
    
    // Preset configurations
    Builder& conservative() noexcept;
    Builder& aggressive() noexcept;
    
    // Build the searcher
    Searcher build(tt::TranspositionTable& tt) noexcept;
    
private:
    SearchState state_;
    SearchConfig config_;
};

} // namespace engine

#endif // SEARCHER_HPP
