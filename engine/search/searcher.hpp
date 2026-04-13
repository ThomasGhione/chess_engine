#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../eval_constants.hpp"
#include "../movelist.hpp"

namespace engine {

class Searcher final {
public:
    Searcher() = delete;

    static constexpr int32_t NEG_INF = std::numeric_limits<int32_t>::min();
    static constexpr int32_t POS_INF = std::numeric_limits<int32_t>::max();
    static constexpr int32_t DEFAULT_DEPTH = 10;
    static constexpr int32_t MAX_PLY = 64;
    static constexpr int32_t CAPTURE_HISTORY_SLOTS = 2;

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

        // External coordination hooks.
        TranspositionTable* transpositionTable = nullptr;
        std::atomic<bool>* stopSearchRequested = nullptr;
        std::atomic<bool>* ponderingStopRequested = nullptr;
        std::atomic<bool>* searchInterrupted = nullptr;

        // Tunable ordering penalty kept for compatibility with Engine policy.
        int32_t orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING;
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
        uint64_t targetDepth,
        bool allowStop) noexcept;

    static chess::Board::Move getBestMove(
        chess::Board& rootBoard,
        const MoveList<chess::Board::Move>& moves,
        bool searchBestMoveForWhite,
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
        bool allowNullMove = true) noexcept;

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

    static constexpr int32_t initialBest(bool isWhite) noexcept;
    static constexpr bool isBetter(int32_t newScore, int32_t currentBest, bool isWhite) noexcept;
    static bool isBetaCutoff(int32_t score, int32_t alpha, int32_t beta, bool isWhite) noexcept;
    static void updateBound(int32_t score, int32_t& alpha, int32_t& beta, bool isWhite) noexcept;
    static bool shouldDeltaPrune(int32_t standPat, int32_t margin, int32_t alpha, int32_t beta, bool isWhite) noexcept;
    static int32_t cutoffValue(int32_t alpha, int32_t beta, bool isWhite) noexcept;
    static bool shouldResearchPVS(int32_t score, int32_t alphaBound, int32_t betaBound, bool isWhite) noexcept;
    static void toTTProbeBounds(int32_t alpha, int32_t beta, int32_t& ttAlpha, int32_t& ttBeta) noexcept;
    static int32_t saturatingAdd32(int32_t lhs, int32_t rhs) noexcept;
    static int32_t saturatingSub32(int32_t lhs, int32_t rhs) noexcept;
    static int16_t clampHeuristic16(int32_t value) noexcept;

    static bool shouldAbortSearch(const SearchRuntime& runtime) noexcept;
    static void markInterrupted(SearchRuntime& runtime) noexcept;
    static bool isInterrupted(const SearchRuntime& runtime) noexcept;
    static void clearInterrupted(SearchRuntime& runtime) noexcept;
    static bool hasSearchStopControl(const SearchRuntime& runtime) noexcept;

    static bool isKillerMove(
        const chess::Board::Move& m,
        const chess::Board::Move killerMoves[2][MAX_PLY],
        int ply) noexcept;

    static bool isPromotionMove(const chess::Board& board, const chess::Board::Move& move) noexcept;
    static bool isEnPassantCapture(const chess::Board& board, const chess::Board::Move& move) noexcept;
    static bool doMoveWithPromotion(chess::Board& b, const chess::Board::Move& m, chess::Board::MoveState& state) noexcept;

    static int32_t stalemateScoreFromMaterialDelta(int32_t matDelta) noexcept;
    static int32_t repetitionDrawScore(const chess::Board& b) noexcept;
    static bool hasInsufficientMaterialDraw(const chess::Board& b) noexcept;

    static void rootNullWindow(bool usIsWhite, int32_t alpha, int32_t beta, int32_t& outAlpha, int32_t& outBeta) noexcept;
    static void updateMinMax(
        bool usIsWhite,
        int32_t score,
        int32_t& alpha,
        int32_t& beta,
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
        uint64_t hashKey) noexcept;

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
        chess::Board& b,
        const SearchNodeState& node,
        int32_t depth,
        int32_t alpha,
        int32_t beta,
        int ply,
        int32_t& outScore) noexcept;

    static SearchMoveResult searchMoves(
        chess::Board& b,
        MoveList<chess::Board::Move>& orderedMoves,
        int32_t* moveScores,
        bool usIsWhite,
        const SearchContext& ctx,
        AlphaBeta& bounds,
        SearchRuntime& runtime,
        bool useTT,
        bool allowHeuristicUpdates,
        bool allowTTWrite) noexcept;

    static void updateKillerAndHistoryOnBetaCutoff(
        const chess::Board& b,
        const chess::Board::Move& m,
        int32_t depth,
        int ply,
        uint8_t us,
        SearchRuntime& runtime,
        const chess::Board::Move* previousMove) noexcept;

    static void storeRootHashMove(
        const chess::Board& rootBoard,
        const chess::Board::Move& move,
        uint64_t depth,
        int32_t score,
        SearchRuntime& runtime,
        uint8_t flag = TranspositionTable::Entry::EXACT) noexcept;
};

} // namespace engine
