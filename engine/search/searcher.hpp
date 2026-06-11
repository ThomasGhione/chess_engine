#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../eval_constants.hpp"
#include "../movelist.hpp"
#include "../syzygy/syzygy.hpp"
#include "sorter.hpp"

namespace engine {

namespace time { class TimeManager; }

// Hoisted to namespace scope so SearchRuntime and other TUs (sorter) can use
// them without depending on the full Searcher class definition.
// Aliased back inside Searcher to keep Searcher::MAX_PLY etc. call sites working.
inline constexpr int32_t  MAX_PLY               = 64;
inline constexpr int32_t  CAPTURE_HISTORY_SLOTS = 2;
inline constexpr int32_t  PAWN_CORR_HISTORY_SIZE = 1 << 14;
inline constexpr uint64_t DEFAULT_DEPTH         = 11;

struct SearchRuntime {
    // --- Search state ---
    uint64_t nodesSearched = 0;
    uint64_t depth         = DEFAULT_DEPTH;
    int32_t  eval          = 0;
    int      maxThreads    = 1;
    // UCI `go nodes N`: 0 = unlimited. Checked per-node against
    // (runtime.nodesSearched + *counter), so the total across IDS iterations
    // is bounded. In YBWC each worker also bounds itself by the same value;
    // total nodes are at most ~maxNodes * threads in multi-threaded searches.
    uint64_t maxNodes      = 0;

    // --- Heuristics ---
    chess::Board::Move killerMoves[2][MAX_PLY] {};
    int16_t  history[2][64][64] {};
    uint16_t counterMoves[64][64] {};
    int16_t  captureHistory[2][64][7][CAPTURE_HISTORY_SLOTS] {};
    int16_t  contHist[2][64][64] {};
    // Correction history: (search - static eval) residual keyed by a board
    // sub-structure. Pawn structure plus minor (N+B) and major (R+Q) skeletons
    // give three semi-independent signals blended into the static eval.
    int16_t  pawnCorrHist[2][PAWN_CORR_HISTORY_SIZE] {};
    int16_t  minorCorrHist[2][PAWN_CORR_HISTORY_SIZE] {};
    int16_t  majorCorrHist[2][PAWN_CORR_HISTORY_SIZE] {};
    // evalStack is thread_local in searchPosition — NOT here: Lazy-SMP races
    // on a shared array would corrupt the `improving` hard-prune heuristic.

    // --- External coordination ---
    TranspositionTable*  transpositionTable    = nullptr;
    std::atomic<bool>*   stopSearchRequested   = nullptr;
    std::atomic<bool>*   ponderingStopRequested = nullptr;
    std::atomic<bool>*   searchInterrupted     = nullptr;
    // Null when running fixed-depth / ponder / perft-style searches.
    time::TimeManager*        timeManager      = nullptr;
    syzygy::SyzygyProber*     syzygyProber     = nullptr;

    [[nodiscard]] bool shouldAbort() const noexcept;
    void markInterrupted() noexcept;
    [[nodiscard]] bool isInterrupted() const noexcept;
    void clearInterrupted() noexcept;
    void softResetHistory() noexcept;
};

class Searcher final {
public:
    Searcher() = delete;

    // Backward-compat aliases.
    static constexpr int32_t  POS_INF               = std::numeric_limits<int32_t>::max();
    // Negamax-safe: NEG_INF == -POS_INF so -(NEG_INF) == POS_INF is valid
    // (negating std::numeric_limits<int32_t>::min() is undefined behaviour).
    static constexpr int32_t  NEG_INF               = -POS_INF;
    static constexpr uint64_t DEFAULT_DEPTH         = ::engine::DEFAULT_DEPTH;
    static constexpr int32_t  MAX_PLY               = ::engine::MAX_PLY;
    static constexpr int32_t  CAPTURE_HISTORY_SLOTS = ::engine::CAPTURE_HISTORY_SLOTS;
    using SearchRuntime = ::engine::SearchRuntime;

    // --- Search constants ---
    static constexpr int     NULL_MOVE_VERIFICATION_DEPTH  = 10;
    static constexpr int32_t MATE_BOUND                    = POS_INF - 2048;

    // --- Qsearch delta pruning ---
    static constexpr int32_t QSEARCH_PAWN_PROMO_DELTA          = 150;
    static constexpr int32_t QSEARCH_MATERIAL_BAD              = -400;
    static constexpr int32_t QSEARCH_MATERIAL_WORSE            = -200;
    static constexpr int32_t QSEARCH_MATERIAL_BAD_DELTA        = 150;
    static constexpr int32_t QSEARCH_MATERIAL_WORSE_DELTA      = 75;
    static constexpr int32_t QSEARCH_DEPTH_REDUCTION_THRESHOLD = 5;
    static constexpr int32_t QSEARCH_DEPTH_REDUCTION_PER_5     = 50;
    static constexpr int32_t QSEARCH_DELTAMARGIN_MIN           = 960;  // QUEEN_VALUE

    // --- Near-promotion ranks ---
    static constexpr uint64_t WHITE_NEAR_PROMO_PAWNS = 0x00FF000000000000ULL;
    static constexpr uint64_t BLACK_NEAR_PROMO_PAWNS = 0x000000000000FF00ULL;

    // --- YBWC (root parallelism) ---
    static constexpr int YBWC_MIN_MOVES = 10;
    static constexpr int YBWC_MIN_DEPTH = DEFAULT_DEPTH - 2;

    // --- Result structs ---
    struct SearchMoveResult {
        chess::Board::Move move;
        int32_t            score;
    };

    struct IterativeSearchResult {
        bool     hasLegalMoves      = false;
        bool     completedAnyDepth  = false;
        bool     terminalRoot       = false;
        uint64_t startDepth         = 0;
        uint64_t targetDepth        = 0;
        uint64_t completedIterations = 0;
        uint64_t completedDepth     = 0;
        uint64_t completedEvenDepth = 0;
        uint64_t interruptedDepth   = 0;
        uint32_t aspirationResearches = 0;
        uint32_t aspirationFailLow    = 0;
        uint32_t aspirationFailHigh   = 0;
        TranspositionTable::Entry::Flag rootScoreBound = TranspositionTable::Entry::EXACT;
        chess::Board::Move bestMove{};
        int32_t            bestScore = 0;
    };

    // --- Public interface ---
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
        int32_t beta  = POS_INF) noexcept;

    static int32_t searchPosition(
        chess::Board& b,
        SearchRuntime& runtime,
        int32_t depth,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool useTT                          = true,
        bool allowTTWrite                   = true,
        bool allowHeuristicUpdates          = true,
        const chess::Board::Move* previousMove = nullptr,
        uint64_t* nodeCounter               = nullptr,
        bool allowNullMove                  = true,
        chess::Board::Move excludedMove     = {}) noexcept;

    static int32_t quiescenceSearch(
        chess::Board& b,
        SearchRuntime& runtime,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool useTT            = true,
        uint64_t* nodeCounter = nullptr,
        bool allowTTWrite     = true) noexcept;

private:
    // --- Private context structs ---
    struct SearchContext {
        int32_t depth;
        int     ply;
        uint8_t activeColor;
        const chess::Board::Move* previousMove = nullptr;
        int32_t  staticEval        = 0;
        bool     inCheck           = false;
        bool     isPVNode          = false;
        uint64_t* nodeCounter      = nullptr;
        int      singularExtension = 0;
        int16_t* contHistEntry     = nullptr;
        bool     iirActive         = false;
        bool     improving         = false;
        chess::Board::Move excludedMove = {};
    };

    struct AlphaBeta {
        int32_t alpha;
        int32_t beta;
    };

    struct SearchNodeState {
        uint8_t activeColor             = chess::Board::WHITE;
        bool    usIsWhite               = true;
        bool    inCheck                 = false;
        bool    isPVNode                = false;
        bool    isPawnEndgameForPruning = false;
        int32_t staticEval              = 0;
    };

    // --- Score utilities ---
    static constexpr bool    isBetter(int32_t newScore, int32_t currentBest) noexcept;
    static constexpr bool    isBetaCutoff(int32_t score, int32_t beta) noexcept;
    static void              updateBound(int32_t score, int32_t& alpha) noexcept;
    static constexpr bool    shouldDeltaPrune(int32_t standPat, int32_t margin, int32_t alpha) noexcept;
    static constexpr bool    shouldResearchPVS(int32_t score, int32_t alphaBound) noexcept;
    static constexpr int32_t saturatingAdd32(int32_t lhs, int32_t rhs) noexcept;
    static constexpr int32_t saturatingSub32(int32_t lhs, int32_t rhs) noexcept;
    // Mate scores embed ply-distance; rebase on TT store/load so mate distances
    // stay correct across transpositions.
    static constexpr int32_t scoreToTT(int32_t score, int ply) noexcept;
    static constexpr int32_t scoreFromTT(int32_t score, int ply) noexcept;
    // delta > 0 is a bonus, delta < 0 a malus; decay uses |delta|.
    static void applyHistoryGravity(int16_t& cell, int32_t delta, int32_t maxValue) noexcept;

    // --- TT helpers ---
    // The move-less overload must NOT forward a 0 move (would clobber an existing stored move).
    static void writeTT(SearchRuntime& runtime, uint64_t hashKey, int32_t depth,
                        int32_t best, int32_t alphaOrig, int32_t betaOrig, int ply) noexcept;
    static void writeTT(SearchRuntime& runtime, uint64_t hashKey, int32_t depth,
                        int32_t best, int32_t alphaOrig, int32_t betaOrig, int ply,
                        const chess::Board::Move& bestMove) noexcept;

    // --- Terminal condition checks ---
    static bool checkEarlyTerminalConditions(
        const chess::Board& b, SearchRuntime& runtime, int ply, int32_t& outScore) noexcept;
    // atRoot=true restricts terminals to *forced* draws (3rd repetition,
    // 50-move, insufficient material). The "2nd repetition scores as 0"
    // pruning rule is an interior-node heuristic only: at the root it would
    // abandon the search and play a random legal move, so it is suppressed.
    static bool checkDrawTerminalConditions(
        const chess::Board& b, int32_t& outScore, bool atRoot = false) noexcept;

    // --- Draw scoring ---
    static int32_t stalemateScoreFromMaterialDelta(int32_t matDelta) noexcept;
    static int32_t drawAdvantageScore(const chess::Board& b) noexcept;
    static int32_t repetitionDrawScore(const chess::Board& b) noexcept;

    // --- Root search helpers ---
    static void rootNullWindow(int32_t alpha, int32_t& outAlpha, int32_t& outBeta) noexcept;
    static void updateMinMax(int32_t score, int32_t& alpha, int32_t& bestScore,
                             chess::Board::Move& bestMove, const chess::Board::Move& m) noexcept;
    static int32_t searchRootMoveScore(
        chess::Board& b,
        const chess::Board::Move& m,
        SearchRuntime& runtime,
        int32_t alpha, int32_t beta,
        bool allowTTWrite,
        bool allowHeuristicUpdates,
        uint64_t* nodeCounter) noexcept;
    static void storeRootHashMove(
        const chess::Board& rootBoard,
        const chess::Board::Move& move,
        uint64_t depth,
        int32_t score,
        SearchRuntime& runtime,
        uint8_t flag = TranspositionTable::Entry::EXACT) noexcept;

    // --- Internal search primitives ---
    static bool handleSearchPrelude(
        const SearchRuntime& runtime, int32_t depth, const AlphaBeta& bounds,
        int32_t& score, uint64_t hashKey, int ply) noexcept;
    static bool tryNullMovePruning(
        chess::Board& b, const SearchNodeState& node, SearchRuntime& runtime,
        int32_t depth, int32_t alpha, int32_t beta, int ply,
        bool useTT, bool allowTTWrite, bool allowHeuristicUpdates,
        uint64_t* nodeCounter, int32_t& outScore) noexcept;
    static bool tryReverseFutilityPruning(
        const chess::Board& b, const SearchNodeState& node,
        int32_t depth, int32_t beta, int32_t& outScore) noexcept;
    static SearchMoveResult searchMoves(
        chess::Board& b, Sorter::MovePickerData& movePicker,
        const SearchContext& ctx, AlphaBeta& bounds,
        SearchRuntime& runtime,
        bool useTT, bool allowHeuristicUpdates, bool allowTTWrite) noexcept;
    static void updateKillerAndHistoryOnBetaCutoff(
        const chess::Board::Move& m, bool isCapture, int victimType,
        int32_t depth, int ply, uint8_t us, SearchRuntime& runtime,
        const chess::Board::Move* previousMove,
        int16_t* contHistEntry = nullptr) noexcept;
};

} // namespace engine
