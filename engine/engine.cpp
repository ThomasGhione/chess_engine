#include "engine.hpp"

#include <algorithm>

#include <omp.h>

#include "../nnue/nnue.hpp"

namespace engine {

namespace {

[[nodiscard]] bool resolveSearchApiMutexGuardDefault() noexcept {
    const char* envValue = std::getenv("CHESS_ENGINE_SEARCH_MUTEX_GUARD");
    if (envValue == nullptr) return true;

    // Any value other than an explicit off-switch (incl. empty) enables the guard.
    const std::string_view value(envValue);
    return !(ascii::iequals(value, "0") || ascii::iequals(value, "off") || ascii::iequals(value, "false"));
}

} // namespace

void Engine::bindSearchRuntime() noexcept {
    searchRuntime.transpositionTable = &tt;
    searchRuntime.stopSearchRequested = &stopSearchRequested;
    searchRuntime.searchInterrupted = &searchInterrupted;
    searchRuntime.syzygyProber = &syzygyProber;
}

void Engine::ensureMagicTablesInitialized() noexcept {
    if (!magicTablesInitialized) {
        pieces::initMagicBitboards();
        magicTablesInitialized = true;
    }
}

Engine::Engine() {

    ensureMagicTablesInitialized();

    // NNUE is the only evaluator: guarantee an active network before any
    // search from this Engine. Test harnesses construct Engine directly and
    // bypass main()'s activation; searching with no network would crash.
    // The board member is built before this body runs, so refresh its
    // accumulator after activating.
    if (!NNUE::networkLoaded() && NNUE::activateEmbedded()) {
        this->board.refreshNnueAccumulator();
    }

    searchApiMutexEnabled.store(resolveSearchApiMutexGuardDefault(), std::memory_order_relaxed);
    searchRuntime.maxThreads = omp_get_max_threads();

    bindSearchRuntime();
    this->tt.clear();
}

void Engine::reset() noexcept {
    // A watchdog still running from an aborted search would stop the next one.
    timeManager.stop();
    board = chess::Board();
    searchRuntime = SearchRuntime{};
    searchRuntime.maxThreads = (requestedThreads > 0) ? requestedThreads : omp_get_max_threads();
    bindSearchRuntime();
    tt.clear();
}

void Engine::setSearchApiMutexEnabled(bool enabled) noexcept {
    this->searchApiMutexEnabled.store(enabled, std::memory_order_release);
}

bool Engine::isSearchApiMutexEnabled() const noexcept {
    return this->searchApiMutexEnabled.load(std::memory_order_acquire);
}

void Engine::prepareSearch() noexcept {
    stopSearchRequested.store(false, std::memory_order_relaxed);
    searchInterrupted.store(false, std::memory_order_relaxed);
    timeManager.clearPonderhit();
}

chess::Move Engine::search(const time::Limits& limits) noexcept {
    std::unique_lock<std::mutex> guard(searchApiMutex, std::defer_lock);
    if (searchApiMutexEnabled.load(std::memory_order_acquire)) guard.lock();

    timeManager.init(limits, board.getActiveColor() == chess::Board::WHITE,
                     std::max(0, board.getFullMoveClock() - 1), &stopSearchRequested);

    // A depth limit keeps its cap; a time-managed, infinite or node-bounded
    // search deepens until stopped. A bare `go` (or `go ponder` without a clock)
    // has nothing that would stop it, so it gets DEFAULT_DEPTH.
    const int targetDepth = (limits.maxDepth > 0) ? static_cast<int>(limits.maxDepth)
        : (timeManager.useTimeManagement() || limits.infinite || limits.maxNodes > 0) ? MAX_PLY
        : DEFAULT_DEPTH;

    searchRuntime.timeManager = &timeManager;
    searchRuntime.maxNodes    = limits.maxNodes;
    timeManager.start();

    chess::Board searchBoard = board;
    const chess::Move best = Searcher::searchBestMove(searchBoard, searchRuntime, targetDepth);

    timeManager.stop();
    searchRuntime.timeManager = nullptr;
    searchRuntime.maxNodes    = 0;
    return best;
}

} // namespace engine
