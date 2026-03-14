#include "../engine.hpp"

#include "searcher.hpp"

namespace engine {

int32_t Engine::searchPosition(
    chess::Board& b,
    int32_t depth,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool useTT,
    bool allowTTWrite,
    bool allowHeuristicUpdates,
    const chess::Board::Move* previousMove,
    uint64_t* nodeCounter,
    bool allowNullMove) noexcept {
    Searcher::SearchRuntime runtime{};
    runtime.nodesSearched = this->nodesSearched;
    runtime.depth = this->depth;
    runtime.eval = this->eval;
    runtime.maxThreads = this->MAX_THREADS;
    std::memcpy(runtime.killerMoves, this->killerMoves, sizeof(runtime.killerMoves));
    std::memcpy(runtime.history, this->history, sizeof(runtime.history));
    std::memcpy(runtime.counterMoves, this->counterMoves, sizeof(runtime.counterMoves));
    std::memcpy(runtime.captureHistory, this->captureHistory, sizeof(runtime.captureHistory));
    runtime.transpositionTable = &this->tt;
    runtime.stopSearchRequested = &this->stopSearchRequested;
    runtime.ponderingStopRequested = &this->ponderingStopRequested;
    runtime.searchInterrupted = &this->searchInterrupted;
    runtime.orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING;

    const int32_t delegatedScore = Searcher::searchPosition(
        b,
        runtime,
        depth,
        alpha,
        beta,
        ply,
        useTT,
        allowTTWrite,
        allowHeuristicUpdates,
        previousMove,
        nodeCounter,
        allowNullMove);

    if (nodeCounter == nullptr) {
        this->nodesSearched = runtime.nodesSearched;
    }
    this->depth = runtime.depth;
    this->eval = runtime.eval;
    std::memcpy(this->killerMoves, runtime.killerMoves, sizeof(this->killerMoves));
    std::memcpy(this->history, runtime.history, sizeof(this->history));
    std::memcpy(this->counterMoves, runtime.counterMoves, sizeof(this->counterMoves));
    std::memcpy(this->captureHistory, runtime.captureHistory, sizeof(this->captureHistory));

    return delegatedScore;
}

} // namespace engine
