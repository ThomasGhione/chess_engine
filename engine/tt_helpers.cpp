#include "engine.hpp"

namespace engine {

// Helper to probe transposition table and return cached score if valid
bool Engine::probeTTCache(uint64_t hashKey, int64_t depth, const AlphaBeta& bounds, int64_t& score) {
    if (depth < 1) return false;

#ifdef DEBUG
    ++ttProbes;
#endif

    const int16_t alpha16 = static_cast<int16_t>(
        std::max<int64_t>(bounds.alpha - TTEntry::ADJUSTMENT, INT16_MIN + 1));
    const int16_t beta16 = static_cast<int16_t>(
        std::min<int64_t>(bounds.beta + TTEntry::ADJUSTMENT, INT16_MAX - 1));

    int16_t ttScore = 0;
    if (probeTT(this->ttTable, hashKey, static_cast<uint8_t>(depth), alpha16, beta16, ttScore)) {
#ifdef DEBUG
        ++ttHits;
        ++ttExactHits;
        ++ttCutoffHits;
#endif
        score = static_cast<int64_t>(ttScore);
        return true;
    }
    return false;
}

// Helper to store position in transposition table
void Engine::saveTTEntry(const TTSaveInfo& info) {
    uint8_t flag = TTEntry::EXACT;
    if (info.score <= info.alphaOrig) {
        flag = TTEntry::UPPERBOUND;  // fail-low
    } else if (info.score >= info.beta) {
        flag = TTEntry::LOWERBOUND;  // fail-high
    }

    const int16_t storedScore = static_cast<int16_t>(
        std::max<int64_t>(
            std::min<int64_t>(info.score, INT16_MAX - 1),
            INT16_MIN + 1));

    storeTTEntry(this->ttTable, info.hashKey, static_cast<uint8_t>(info.depth), storedScore, flag);
}

}  // namespace engine
