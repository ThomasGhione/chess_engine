#include "searcher.hpp"

namespace engine {

// ============================================================================
// Builder implementation
// ============================================================================

Searcher::Builder& Searcher::Builder::withDepth(uint8_t depth) noexcept {
    config_.maxDepth = depth;
    return *this;
}

Searcher::Builder& Searcher::Builder::withThreads(int threads) noexcept {
    config_.maxThreads = threads;
    return *this;
}

Searcher::Builder& Searcher::Builder::withConfig(const SearchConfig& config) noexcept {
    config_ = config;
    return *this;
}

Searcher::Builder& Searcher::Builder::disableTT() noexcept {
    config_.useTT = false;
    return *this;
}

Searcher::Builder& Searcher::Builder::disableLMR() noexcept {
    config_.useLMR = false;
    return *this;
}

Searcher::Builder& Searcher::Builder::disableNullMove() noexcept {
    config_.useNullMove = false;
    return *this;
}

Searcher::Builder& Searcher::Builder::disablePruning() noexcept {
    config_.useFutilityPruning = false;
    config_.useLateMoveReductions = false;
    return *this;
}

Searcher::Builder& Searcher::Builder::disableAspirationWindows() noexcept {
    config_.useAspirationWindows = false;
    return *this;
}

Searcher::Builder& Searcher::Builder::conservative() noexcept {
    config_ = SearchConfig::conservative();
    return *this;
}

Searcher::Builder& Searcher::Builder::aggressive() noexcept {
    config_ = SearchConfig::aggressive();
    return *this;
}

Searcher Searcher::Builder::build(tt::TranspositionTable& tt) noexcept {
    return Searcher(state_, config_, tt);
}

} // namespace engine
