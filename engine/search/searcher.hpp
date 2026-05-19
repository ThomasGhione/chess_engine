#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../eval_constants.hpp"
#include "../movelist.hpp"
#include "sorter.hpp"

namespace engine {

namespace time { class TimeManager; }

class Searcher final {
public:
    Searcher() = delete;

    static constexpr int32_t POS_INF = std::numeric_limits<int32_t>::max();
    // Negamax-safe: NEG_INF == -POS_INF so -(NEG_INF) == POS_INF is valid
    // (negating std::numeric_limits<int32_t>::min() is undefined behaviour).
    static constexpr int32_t NEG_INF = -POS_INF;
    static constexpr int32_t DEFAULT_DEPTH = 11;
    static constexpr int32_t MAX_PLY = 64;
    static constexpr int32_t CAPTURE_HISTORY_SLOTS = 2;

    // Null move pruning thresholds
    static constexpr int NULL_MOVE_VERIFICATION_DEPTH = 10;  // Verify null move if depth >= 10

    // Quiescence search delta pruning thresholds
    static constexpr int32_t QSEARCH_PAWN_PROMO_DELTA = 150;      // Bonus for near-promotion pawns
    static constexpr int32_t QSEARCH_MATERIAL_BAD = -400;         // Material balance threshold (bad)
    static constexpr int32_t QSEARCH_MATERIAL_WORSE = -200;       // Material balance threshold (worse)
    static constexpr int32_t QSEARCH_MATERIAL_BAD_DELTA = 150;    // Delta margin if bad
    static constexpr int32_t QSEARCH_MATERIAL_WORSE_DELTA = 75;   // Delta margin if worse
    static constexpr int32_t QSEARCH_DEPTH_REDUCTION_THRESHOLD = 5;  // Reduce deltaMargin if qsearchDepth > 5
    static constexpr int32_t QSEARCH_DEPTH_REDUCTION_PER_5 = 50;  // Reduce by 50cp per 5 plies
    static constexpr int32_t QSEARCH_DELTAMARGIN_MIN = 960;  // Minimum delta margin (QUEEN_VALUE)

    // Pawn promotion bitboards (rank 7/2 for white/black)
    static constexpr uint64_t WHITE_NEAR_PROMO_PAWNS = 0x00FF000000000000ULL;  // Rank 7 (row 2)
    static constexpr uint64_t BLACK_NEAR_PROMO_PAWNS = 0x000000000000FF00ULL;  // Rank 2 (row 7)

    // Root search parallelization thresholds
    static constexpr int YBWC_MIN_MOVES = 10;        // Min moves to enable YBWC
    static constexpr int YBWC_MIN_DEPTH = DEFAULT_DEPTH - 2;  // Min depth to enable YBWC

    struct SearchMoveResult {
        chess::Board::Move move;
        int32_t score;
    };

    struct SearchRuntime {
        // Search state
        uint64_t nodesSearched = 0;
        uint64_t depth = DEFAULT_DEPTH;
        int32_t eval = 0;
        int maxThreads = 1;

        // Heuristics state (same layout used by Engine search path).
        chess::Board::Move killerMoves[2][MAX_PLY] {};
        int16_t history[2][64][64] {};
        uint16_t counterMoves[64][64] {};
        int16_t captureHistory[2][64][7][CAPTURE_HISTORY_SLOTS] {};
        int16_t contHist[2][64][64] {};
        // evalStack is per-thread (thread_local in searchPosition), NOT here:
        // it feeds a hard prune (`improving`) so a Lazy-SMP race on a shared
        // array could corrupt pruning and change the chosen move.

        // External coordination hooks.
        TranspositionTable* transpositionTable = nullptr;
        std::atomic<bool>* stopSearchRequested = nullptr;
        std::atomic<bool>* ponderingStopRequested = nullptr;
        std::atomic<bool>* searchInterrupted = nullptr;

        // Optional: drives soft-limit / stability decisions in iterative
        // deepening. Null for fixed-depth / ponder / perft-style searches.
        time::TimeManager* timeManager = nullptr;

    };

    struct IterativeSearchResult {
        bool hasLegalMoves = false;
        bool completedAnyDepth = false;
        bool terminalRoot = false;
        uint64_t startDepth = 0;
        uint64_t targetDepth = 0;
        uint64_t completedIterations = 0;
        uint64_t completedDepth = 0;
        uint64_t completedEvenDepth = 0;
        uint64_t interruptedDepth = 0;
        uint32_t aspirationResearches = 0;
        uint32_t aspirationFailLow = 0;
        uint32_t aspirationFailHigh = 0;
        TranspositionTable::Entry::Flag rootScoreBound = TranspositionTable::Entry::EXACT;
        chess::Board::Move bestMove{};
        int32_t bestScore = 0;
    };

    static void softResetHistory(SearchRuntime& runtime) noexcept;

    static chess::Board::Move searchBestMove(
        chess::Board& board,
        SearchRuntime& runtime,
        uint64_t requestedDepth = DEFAULT_DEPTH) noexcept;

    static IterativeSearchResult runIterativeDeepening(
        chess::Board& rootBoard,
        SearchRuntime& runtime,
        uint64_t startDepth,
        uint64_t targetDepth) noexcept;

    static chess::Board::Move getBestMove(
        chess::Board& rootBoard,
        const MoveList<chess::Board::Move>& moves,
        SearchRuntime& runtime,
        int32_t alpha = NEG_INF,
        int32_t beta = POS_INF) noexcept;

    static int32_t searchPosition(
        chess::Board& b,
        SearchRuntime& runtime,
        int32_t depth,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool useTT = true,
        bool allowTTWrite = true,
        bool allowHeuristicUpdates = true,
        const chess::Board::Move* previousMove = nullptr,
        uint64_t* nodeCounter = nullptr,
        bool allowNullMove = true,
        chess::Board::Move excludedMove = {}) noexcept;

    static int32_t quiescenceSearch(
        chess::Board& b,
        SearchRuntime& runtime,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool useTT = true,
        uint64_t* nodeCounter = nullptr,
        bool allowTTWrite = true) noexcept;

private:
    struct SearchContext {
        int32_t depth;
        int ply;
        uint8_t activeColor;
        const chess::Board::Move* previousMove = nullptr;
        int32_t staticEval = 0;
        bool inCheck = false;
        bool isPVNode = false;
        uint64_t* nodeCounter = nullptr;
        int singularExtension = 0;
        int16_t* contHistEntry = nullptr;
        bool iirActive = false;
        bool improving = false;
        chess::Board::Move excludedMove = {};
    };

    struct AlphaBeta {
        int32_t alpha;
        int32_t beta;
    };

    struct SearchNodeState {
        uint8_t activeColor = chess::Board::WHITE;
        bool usIsWhite = true;
        bool inCheck = false;
        bool isPVNode = false;
        bool isPawnEndgameForPruning = false;
        int32_t staticEval = 0;
    };

    static constexpr int32_t initialBest() noexcept;
    static constexpr bool isBetter(int32_t newScore, int32_t currentBest) noexcept;
    static constexpr bool isBetaCutoff(int32_t score, int32_t beta) noexcept;
    static void updateBound(int32_t score, int32_t& alpha) noexcept;
    static constexpr bool shouldDeltaPrune(int32_t standPat, int32_t margin, int32_t alpha) noexcept;
    static constexpr int32_t cutoffValue(int32_t beta) noexcept;
    static constexpr bool shouldResearchPVS(int32_t score, int32_t alphaBound) noexcept;
    static constexpr int32_t saturatingAdd32(int32_t lhs, int32_t rhs) noexcept;
    static constexpr int32_t saturatingSub32(int32_t lhs, int32_t rhs) noexcept;
    // Mate scores are ±INF∓ply (ply = distance from root); the TT is keyed by
    // position only, so they must be made node-relative on store and re-based
    // to the probing node's ply on read, else mate distances are wrong across
    // transpositions.
    static constexpr int32_t MATE_BOUND = POS_INF - 2048;
    static constexpr int32_t scoreToTT(int32_t score, int ply) noexcept;
    static constexpr int32_t scoreFromTT(int32_t score, int ply) noexcept;
    static constexpr int16_t clampHeuristic16(int32_t value) noexcept;
    // Gravity update shared by history / continuation / capture history.
    // delta > 0 is a bonus, delta < 0 a malus; decay uses |delta|.
    static void applyHistoryGravity(int16_t& cell, int32_t delta, int32_t maxValue) noexcept;
    // TT store wrappers: flag derivation + mate-score rebasing + clamping.
    // The move-less overload must NOT forward a 0 move (that would clobber an
    // existing stored move via replaceBestMove=true).
    static void writeTT(SearchRuntime& runtime, uint64_t hashKey, int32_t depth,
                        int32_t best, int32_t alphaOrig, int32_t betaOrig, int ply) noexcept;
    static void writeTT(SearchRuntime& runtime, uint64_t hashKey, int32_t depth,
                        int32_t best, int32_t alphaOrig, int32_t betaOrig, int ply,
                        const chess::Board::Move& bestMove) noexcept;

    static bool shouldAbortSearch(const SearchRuntime& runtime) noexcept;
    static void markInterrupted(SearchRuntime& runtime) noexcept;
    static bool isInterrupted(const SearchRuntime& runtime) noexcept;
    static void clearInterrupted(SearchRuntime& runtime) noexcept;
    // Helper: check early terminal conditions (abort, MAX_PLY, king-capture)
    static bool checkEarlyTerminalConditions(
        const chess::Board& b,
        SearchRuntime& runtime,
        int ply,
        int32_t& outScore) noexcept;
    static bool checkDrawTerminalConditions(
        const chess::Board& b,
        int32_t& outScore) noexcept;


    static int32_t stalemateScoreFromMaterialDelta(int32_t matDelta) noexcept;
    static int32_t drawAdvantageScore(const chess::Board& b) noexcept;
    static int32_t repetitionDrawScore(const chess::Board& b) noexcept;

    static void rootNullWindow(int32_t alpha, int32_t& outAlpha, int32_t& outBeta) noexcept;
    static void updateMinMax(
        int32_t score,
        int32_t& alpha,
        int32_t& bestScore,
        chess::Board::Move& bestMove,
        const chess::Board::Move& m) noexcept;

    static int32_t searchRootMoveScore(
        chess::Board& b,
        const chess::Board::Move& m,
        SearchRuntime& runtime,
        int32_t alpha,
        int32_t beta,
        int currPly,
        bool useTT,
        bool allowTTWrite,
        bool allowHeuristicUpdates,
        uint64_t* nodeCounter) noexcept;

    static bool handleSearchPrelude(
        const SearchRuntime& runtime,
        int32_t depth,
        const AlphaBeta& bounds,
        int32_t& score,
        uint64_t hashKey,
        int ply) noexcept;

    static bool tryNullMovePruning(
        chess::Board& b,
        const SearchNodeState& node,
        SearchRuntime& runtime,
        int32_t depth,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool useTT,
        bool allowTTWrite,
        bool allowHeuristicUpdates,
        uint64_t* nodeCounter,
        int32_t& outScore) noexcept;

    static bool tryReverseFutilityPruning(
        const chess::Board& b,
        const SearchNodeState& node,
        int32_t depth,
        int32_t beta,
        int ply,
        int32_t& outScore) noexcept;

    static SearchMoveResult searchMoves(
        chess::Board& b,
        Sorter::MovePickerData& movePicker,
        const SearchContext& ctx,
        AlphaBeta& bounds,
        SearchRuntime& runtime,
        bool useTT,
        bool allowHeuristicUpdates,
        bool allowTTWrite) noexcept;

    static void updateKillerAndHistoryOnBetaCutoff(
        const chess::Board::Move& m,
        bool isCapture,
        int victimType,
        int32_t depth,
        int ply,
        uint8_t us,
        SearchRuntime& runtime,
        const chess::Board::Move* previousMove,
        int16_t* contHistEntry = nullptr) noexcept;

    static void storeRootHashMove(
        const chess::Board& rootBoard,
        const chess::Board::Move& move,
        uint64_t depth,
        int32_t score,
        SearchRuntime& runtime,
        uint8_t flag = TranspositionTable::Entry::EXACT) noexcept;
};

} // namespace engine
