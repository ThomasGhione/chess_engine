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
    static constexpr int       DEFAULT_DEPTH         = ::engine::DEFAULT_DEPTH;
    static constexpr int      MAX_PLY               = ::engine::MAX_PLY;
    static constexpr int      CAPTURE_HISTORY_SLOTS = ::engine::CAPTURE_HISTORY_SLOTS;
    using SearchRuntime = ::engine::SearchRuntime;

    // All tunable search parameters live in search_constants.hpp (namespace
    // engine), referenced unqualified from Searcher's methods.

    // --- Result structs ---
    struct SearchMoveResult {
        chess::Move move;
        int32_t            score;
    };

    struct IterativeSearchResult {
        bool     hasLegalMoves      = false;
        bool     completedAnyDepth  = false;
        bool     terminalRoot       = false;
        int      completedDepth     = 0;
        TranspositionTable::Entry::Flag rootScoreBound = TranspositionTable::Entry::EXACT;
        chess::Move bestMove{};
        int32_t            bestScore = 0;
    };

    // --- Public interface ---
    static chess::Move searchBestMove(
        chess::Board& board,
        SearchRuntime& runtime,
        int requestedDepth = DEFAULT_DEPTH) noexcept;

    static IterativeSearchResult runIterativeDeepening(
        chess::Board& rootBoard,
        SearchRuntime& runtime,
        int startDepth,
        int targetDepth) noexcept;

    static chess::Move getBestMove(
        chess::Board& rootBoard,
        const MoveList& moves,
        SearchRuntime& runtime,
        int32_t alpha = NEG_INF,
        int32_t beta  = POS_INF) noexcept;

    static int32_t searchPosition(
        chess::Board& b,
        SearchRuntime& runtime,
        int depth,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool useTT                          = true,
        bool allowTTWrite                   = true,
        bool allowHeuristicUpdates          = true,
        const chess::Move* previousMove = nullptr,
        uint64_t* nodeCounter               = nullptr,
        bool allowNullMove                  = true,
        chess::Move excludedMove     = {}) noexcept;

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
        const chess::Move* previousMove = nullptr;
        uint64_t* nodeCounter      = nullptr;
        int16_t* contHistEntry     = nullptr;
        int      depth;
        int      ply;
        int32_t  staticEval        = 0;
        int      singularExtension = 0;
        chess::Move excludedMove = {};
        uint8_t activeColor;
        bool     inCheck           = false;
        bool     isPVNode          = false;
        bool     iirActive         = false;
        bool     improving         = false;
    };

    struct SearchNodeState {
        uint8_t activeColor             = chess::Board::WHITE;
        bool    inCheck                 = false;
        bool    isPVNode                = false;
        bool    isPawnEndgameForPruning = false;
        int32_t staticEval              = 0;
    };

    // --- Score utilities ---
    static constexpr bool isBetter(int32_t newScore, int32_t currentBest) noexcept       { return newScore > currentBest; }
    static constexpr bool isBetaCutoff(int32_t score, int32_t beta) noexcept             { return score >= beta; }
    static void           updateBound(int32_t score, int32_t& alpha) noexcept            { if (score > alpha) alpha = score; }
    static constexpr bool shouldDeltaPrune(int32_t standPat, int32_t margin, int32_t alpha) noexcept { return standPat + margin <= alpha; }
    static constexpr bool shouldResearchPVS(int32_t score, int32_t alphaBound) noexcept  { return score > alphaBound; }
    static constexpr int32_t saturatingAdd32(int32_t lhs, int32_t rhs) noexcept;
    static constexpr int32_t saturatingSub32(int32_t lhs, int32_t rhs) noexcept;
    // Mate scores embed ply-distance; rebase on TT store/load so mate distances
    // stay correct across transpositions.
    static constexpr int32_t scoreToTT(int32_t score, int ply) noexcept;
    static constexpr int32_t scoreFromTT(int32_t score, int ply) noexcept;
    // delta > 0 is a bonus, delta < 0 a malus; decay uses |delta|.
    static void applyHistoryGravity(int16_t& cell, int32_t delta, int32_t maxValue) noexcept;

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
    // When `node`'s side has no legal move, sets outScore to the stalemate score
    // and returns true; otherwise returns false. Shared by the NMP/RFP fail-high
    // paths, which must not claim a cutoff on a stalemated node.
    static bool tryStalemateScore(const chess::Board& b, const SearchNodeState& node, int32_t& outScore) noexcept;
    static int32_t drawAdvantageScore(const chess::Board& b) noexcept;
    static int32_t repetitionDrawScore(const chess::Board& b) noexcept;

    // --- Root search helpers ---
    static void updateMinMax(int32_t score, int32_t& alpha, int32_t& bestScore,
                             chess::Move& bestMove, const chess::Move& m) noexcept;
    static int32_t searchRootMoveScore(
        chess::Board& b,
        const chess::Move& m,
        SearchRuntime& runtime,
        int32_t alpha, int32_t beta,
        bool allowTTWrite,
        bool allowHeuristicUpdates,
        uint64_t* nodeCounter) noexcept;
    static void storeRootHashMove(
        const chess::Board& rootBoard,
        const chess::Move& move,
        int depth,
        int32_t score,
        SearchRuntime& runtime,
        uint8_t flag = TranspositionTable::Entry::EXACT) noexcept;

    // --- Internal search primitives ---
    static bool handleSearchPrelude(
        const SearchRuntime& runtime, int depth, int32_t alpha, int32_t beta,
        int32_t& score, uint64_t hashKey, int ply) noexcept;
    static bool tryNullMovePruning(
        chess::Board& b, const SearchNodeState& node, SearchRuntime& runtime,
        int depth, int32_t alpha, int32_t beta, int ply,
        bool useTT, bool allowTTWrite, bool allowHeuristicUpdates,
        uint64_t* nodeCounter, int32_t& outScore) noexcept;
    static bool tryReverseFutilityPruning(
        const chess::Board& b, const SearchNodeState& node,
        int depth, int32_t beta, int32_t& outScore) noexcept;
    static SearchMoveResult searchMoves(
        chess::Board& b, MovePicker& movePicker,
        const SearchContext& ctx, int32_t alpha, int32_t beta,
        SearchRuntime& runtime,
        bool useTT, bool allowHeuristicUpdates, bool allowTTWrite) noexcept;
    static void updateKillerAndHistoryOnBetaCutoff(
        const chess::Move& m, bool isCapture, int victimType,
        int depth, int ply, uint8_t us, SearchRuntime& runtime,
        const chess::Move* previousMove,
        int16_t* contHistEntry = nullptr,
        int fromPieceType = 0) noexcept;
};

} // namespace engine
