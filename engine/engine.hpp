#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <thread>

#include "../board/board.hpp"
#include "../tt/tt.hpp"

#include "search/searcher.hpp"
#include "time/time_manager.hpp"
#include "opening/opening_book.hpp"
#include "syzygy/syzygy.hpp"

namespace engine {



static inline constexpr int32_t POS_INF = std::numeric_limits<int32_t>::max();
// Negamax-safe: NEG_INF == -POS_INF so it can be negated without UB.
static inline constexpr int32_t NEG_INF = -POS_INF;



class Engine final {
public:
    enum class GameResult : uint8_t {
        ONGOING = 0,
        WHITE_WINS = 1,
        BLACK_WINS = 2,
        DRAW = 3
    };

    // Lifecycle
    Engine();
    ~Engine() noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Gameplay API
    void reset() noexcept;
    bool movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece = '\0') noexcept;

    // Search API
    void search(uint64_t depth) noexcept;
    chess::Board::Move searchUCI(uint64_t depth) noexcept;
    chess::Board::Move searchUCI(const time::Limits& limits) noexcept;
    void stopThinking() noexcept;
    void setSearchApiMutexEnabled(bool enabled) noexcept;
    bool isSearchApiMutexEnabled() const noexcept;
    void setPonderDebugEnabled(bool enabled) noexcept;
    bool isPonderDebugEnabled() const noexcept;
    uint64_t getPonderCurrentDepth() const noexcept;
    uint64_t getPonderLastCompletedDepth() const noexcept;
    uint64_t getPonderInterruptedDepth() const noexcept;

    // Game state
    bool isGameOver() const noexcept { return gameResult != GameResult::ONGOING; }
    bool isMate() const noexcept { return gameResult == GameResult::WHITE_WINS || gameResult == GameResult::BLACK_WINS; }
    bool isStalemate() const noexcept { return gameResult == GameResult::DRAW && board.isStalemate(board.getActiveColor()); }
    bool isDraw() const noexcept { return gameResult == GameResult::DRAW; }
    void updateGameResult() noexcept;

    // Public state kept for compatibility with existing call-sites.
    chess::Board::Move bestMove;
    chess::Board board;
    bool isPlayerWhite = true;

    // Unified runtime state owned by Searcher.
    Searcher::SearchRuntime searchRuntime{};

    // Compatibility aliases for existing call-sites.
    uint64_t& depth;
    int32_t& eval;
    uint64_t& nodesSearched;
    int& MAX_THREADS;

    // UCI `Threads` override. 0 = auto (use omp_get_max_threads()). Persists
    // across reset()/ucinewgame so a GUI/cutechess setting survives new games.
    int requestedThreads = 0;

    static constexpr int32_t DEFAULTDEPTH = Searcher::DEFAULT_DEPTH;
    static constexpr int32_t MAX_PLY = Searcher::MAX_PLY;

    static constexpr size_t MOVE_HISTORY_MAX_PLIES = 1024;
    static constexpr size_t MOVE_HISTORY_ENTRY_MAX_LEN = 6; // "e7e8q\n"
    static constexpr size_t MOVE_HISTORY_MAX_BYTES = MOVE_HISTORY_MAX_PLIES * MOVE_HISTORY_ENTRY_MAX_LEN;
    std::string moveHistory {};

    // Opening book
    opening::OpeningBook openingBook;
    std::atomic<bool> openingEnabled {true};

    // Syzygy tablebases
    syzygy::SyzygyProber syzygyProber;

    // Transposition table (shared by normal search and pondering)
    TranspositionTable tt;

    // Per-move time budget + hard-deadline watchdog (UCI clock searches).
    time::TimeManager timeManager;

private:

    GameResult gameResult = GameResult::ONGOING;

    // Search/pondering coordination
    std::thread ponderingThread;
    std::mutex ponderingMutex;
    std::condition_variable ponderingCv;
    chess::Board ponderingBoard {};
    bool ponderingWorkReady = false;
    bool ponderingWorkerStopping = false;
    std::atomic<bool> ponderingStopRequested {false};
    std::atomic<bool> ponderingActive {false};
    std::atomic<bool> stopSearchRequested {false};
    std::atomic<bool> searchInterrupted {false};
    std::mutex searchApiMutex;
    std::atomic<bool> searchApiMutexEnabled {true};
    std::atomic<bool> ponderDebugEnabled {false};
    std::atomic<uint64_t> ponderCurrentDepth {0};
    std::atomic<uint64_t> ponderLastCompletedDepth {0};
    std::atomic<uint64_t> ponderLastCompletedEvenDepth {0};
    std::atomic<uint64_t> ponderInterruptedDepth {0};
    std::atomic<uint32_t> ponderAspirationResearches {0};
    std::atomic<uint32_t> ponderAspirationFailLow {0};
    std::atomic<uint32_t> ponderAspirationFailHigh {0};
    uint64_t ponderRootHash = 0;
    uint64_t ponderResultDepth = 0;
    int32_t ponderResultScore = 0;
    chess::Board::Move ponderResultMove {};
    bool ponderResultReady = false;

    // Shared bitboard init (all Engine instances); only the ctor touches these.
    static inline bool magicTablesInitialized = false;
    static void ensureMagicTablesInitialized() noexcept;

    // Internal helpers
    void bindSearchRuntime() noexcept;
    void appendMoveHistoryEntry(const chess::Coords& from, const chess::Coords& to, char promotionPiece) noexcept;
    void clearPonderResult() noexcept;
    void clearPonderCounters() noexcept;
    std::unique_lock<std::mutex> acquireSearchApiLock() noexcept;
    void requestStopPondering() noexcept;
    bool tryUsePonderResult(uint64_t requestedDepth, chess::Board::Move& outMove) noexcept;
    void startPondering() noexcept;
    void stopPondering() noexcept;
    bool waitForPonderJob(chess::Board& outBoard) noexcept;
    void ponderWorkerLoop() noexcept;
    void ponderLoop(chess::Board&& rootBoard) noexcept;
};

} // namespace engine

#include "inl/bitboard_helpers.inl"
