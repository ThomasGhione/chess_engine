#pragma once

#include <atomic>
#include <cstdint>

#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../syzygy/syzygy.hpp"
#include "search_constants.hpp"

namespace engine {

namespace time { class TimeManager; }

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
    TranspositionTable*       transpositionTable     = nullptr;
    std::atomic<bool>*        stopSearchRequested    = nullptr;
    std::atomic<bool>*        ponderingStopRequested = nullptr;
    std::atomic<bool>*        searchInterrupted      = nullptr;
    // Null when running fixed-depth / ponder / perft-style searches.
    time::TimeManager*        timeManager            = nullptr;
    syzygy::SyzygyProber*     syzygyProber           = nullptr;

    [[nodiscard]] inline bool shouldAbort() const noexcept {
        const bool stopRequested = stopSearchRequested != nullptr
            && stopSearchRequested->load(std::memory_order_acquire);
        const bool ponderStopRequested = ponderingStopRequested != nullptr
            && ponderingStopRequested->load(std::memory_order_acquire);
        return stopRequested || ponderStopRequested;
    }

    inline void markInterrupted() noexcept {
        if (searchInterrupted != nullptr)
            searchInterrupted->store(true, std::memory_order_relaxed);
    }

    [[nodiscard]] inline bool isInterrupted() const noexcept {
        return searchInterrupted != nullptr
            && searchInterrupted->load(std::memory_order_relaxed);
    }

    inline void clearInterrupted() noexcept {
        if (searchInterrupted != nullptr)
            searchInterrupted->store(false, std::memory_order_relaxed);
    }

    inline void softResetHistory() noexcept {
        constexpr int HISTORY_CELLS   = 2 * 64 * 64;
        constexpr int CONT_HIST_CELLS = 2 * 64 * 64;
        constexpr int CAP_HIST_CELLS  = 2 * 64 * 7 * CAPTURE_HISTORY_SLOTS;

        int16_t* historyFlat  = &history[0][0][0];
        int16_t* contHistFlat = &contHist[0][0][0];
        int16_t* capHistFlat  = &captureHistory[0][0][0][0];

        #pragma omp simd
        for (int i = 0; i < HISTORY_CELLS; ++i)   historyFlat[i]  >>= 1;
        #pragma omp simd
        for (int i = 0; i < CONT_HIST_CELLS; ++i) contHistFlat[i] >>= 1;
        #pragma omp simd
        for (int i = 0; i < CAP_HIST_CELLS; ++i)  capHistFlat[i]  >>= 1;
    }
};

} // namespace engine
