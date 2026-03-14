#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#ifdef DEBUG
#include <iostream>
#endif

#include "../board/board.hpp"
#include "../board/coords.hpp"
#include "../tt/tt.hpp"

#include "search/searcher.hpp"

namespace engine {

class Engine final {
public:
    enum GameResult : uint8_t {
        ONGOING = 0,
        WHITE_WINS = 1,
        BLACK_WINS = 2,
        DRAW = 3
    };

    Engine();
    explicit Engine(const std::string& fen);
    ~Engine() noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    void reset() noexcept;
    bool movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece = '\0') noexcept;

    void search(uint64_t depth) noexcept;
    chess::Board::Move searchUCI(uint64_t depth) noexcept;
    void stopThinking() noexcept;
    void setPonderDebugEnabled(bool enabled) noexcept;
    bool isPonderDebugEnabled() const noexcept;
    uint64_t getPonderCurrentDepth() const noexcept;
    uint64_t getPonderLastCompletedDepth() const noexcept;
    uint64_t getPonderInterruptedDepth() const noexcept;
    int32_t evaluate(const chess::Board& board) noexcept;
    int32_t evaluateTrace(const chess::Board& board) noexcept;
    int32_t evaluateCheckmate(const chess::Board& board) noexcept;

    bool isGameOver() const noexcept;
    bool isMate() const noexcept;
    bool isStalemate() const noexcept;
    void updateGameResult() noexcept;
    GameResult getGameResult() const noexcept;
    uint8_t getActiveColor() const noexcept;

    static int32_t getMaterialDelta(const chess::Board& b) noexcept;

    // Magic bitboard initialization (shared across all Engine instances)
    static inline bool magicTablesInitialized = false;
    static void ensureMagicTablesInitialized() noexcept;

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

    static constexpr int32_t DEFAULTDEPTH = Searcher::DEFAULT_DEPTH;
    static constexpr int32_t MAX_PLY = Searcher::MAX_PLY;

    static constexpr size_t MOVE_HISTORY_MAX_PLIES = 1024;
    static constexpr size_t MOVE_HISTORY_ENTRY_MAX_LEN = 6; // "e7e8q\n"
    static constexpr size_t MOVE_HISTORY_MAX_BYTES = MOVE_HISTORY_MAX_PLIES * MOVE_HISTORY_ENTRY_MAX_LEN;
    std::string moveHistory = "";

    // Transposition table
    tt::TranspositionTable tt;

private:
    GameResult gameResult = Engine::ONGOING;

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

    static char promotionChoiceForMove(const chess::Board& board, const chess::Board::Move& move) noexcept;
    void bindSearchRuntime() noexcept;
    void appendMoveHistoryEntry(const chess::Coords& from, const chess::Coords& to, char promotionPiece) noexcept;
    void startPondering() noexcept;
    void stopPondering() noexcept;
    void ponderLoop(chess::Board rootBoard) noexcept;
};

} // namespace engine

#include "inl/bitboard_helpers.inl"
#include "inl/accessors.inl"

#endif
