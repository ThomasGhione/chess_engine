#pragma once

#include <cstdint>
#include <limits>

#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../eval_constants.hpp"
#include "../movelist.hpp"
#include "search_constants.hpp"
#include "searchruntime.hpp"
#include "../sort/sorter.hpp"

namespace engine {

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

    // All tunable search parameters live in search_constants.hpp (namespace
    // engine), referenced unqualified from Searcher's methods.

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
        chess::Board& b, MovePicker& movePicker,
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
