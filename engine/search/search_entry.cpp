#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

namespace {
inline int32_t saturatingAdd32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t sum = static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs);
    if (sum > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) return std::numeric_limits<int32_t>::max();
    if (sum < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(sum);
}

inline int32_t saturatingSub32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t diff = static_cast<int64_t>(lhs) - static_cast<int64_t>(rhs);
    if (diff > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) return std::numeric_limits<int32_t>::max();
    if (diff < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(diff);
}
} // namespace

inline void Engine::rootNullWindow(bool usIsWhite, int32_t alpha, int32_t beta, int32_t& outAlpha, int32_t& outBeta) noexcept {
    outAlpha = usIsWhite ? alpha : saturatingSub32(beta, 1);
    outBeta = usIsWhite ? saturatingAdd32(alpha, 1) : beta;
}

void Engine::updateMinMax(bool usIsWhite, int32_t score, int32_t& alpha, int32_t& beta, int32_t& bestScore, 
                          chess::Board::Move& bestMove, const chess::Board::Move& m) noexcept {
    // Update best score and move if this is better
    if (Engine::isBetter(score, bestScore, usIsWhite)) {
        bestScore = score;
        bestMove = m;
    }
    
    // Update alpha/beta bounds
    updateBound(score, alpha, beta, usIsWhite);
}

int32_t Engine::searchRootMoveScore(chess::Board& b, const chess::Board::Move& m, int32_t alpha, int32_t beta,
                                    int currPly, bool useTT, bool allowTTWrite, bool allowHeuristicUpdates, uint64_t* nodeCounter) noexcept {
    chess::Board::MoveState state;
    doMoveWithPromotion(b, m, state);
    const int32_t score = this->searchPosition(
        b, this->depth - 1, alpha, beta, currPly, useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter);
    b.undoMove(m, state);
    return score;
}

chess::Board::Move Engine::getBestMove(chess::Board& rootBoard, const MoveList<chess::Board::Move>& moves, bool usIsWhite) noexcept {
    return getBestMove(rootBoard, moves, usIsWhite, NEG_INF, POS_INF);
}

chess::Board::Move Engine::getBestMove(chess::Board& rootBoard, const MoveList<chess::Board::Move>& moves, bool usIsWhite, int32_t alpha, int32_t beta) noexcept {
    int32_t bestScore = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = moves[0];
    constexpr int currPly = 1;
    uint64_t localNodes = 0;
    bool searchedAnyMove = false;
    MoveList<chess::Board::Move> orderedRootMoves = moves;
    {
        const uint64_t rootHash = rootBoard.getHash();
        (void)this->sortLegalMoves(orderedRootMoves, 0, rootBoard, usIsWhite, rootHash, nullptr);
    }
    const MoveList<chess::Board::Move>& rootMoves = orderedRootMoves;

    // Parallel YBWC is enabled only when:
    // - enough moves (>= 10) to amortize threading overhead
    // - sufficient depth (>= DEFAULTDEPTH - 2) for real speedup
    const bool useYBWC = (rootMoves.size >= 10 &&
                          this->depth >= (Engine::DEFAULTDEPTH - 2));
    
    if (!useYBWC) {
        // Sequential search with PVS (Principal Variation Search)
        // First move: full window
        // Next moves: null window, then re-search if needed
        
        for (int i = 0; i < rootMoves.size; ++i) {
            if (this->shouldAbortSearch()) {
                this->searchInterrupted.store(true, std::memory_order_relaxed);
                break;
            }

            const auto& m = rootMoves[i];
            int32_t score = 0;
            if (i == 0) {
                // First move: search with full window (PV node)
                score = this->searchRootMoveScore(rootBoard, m, alpha, beta, currPly, true, true, true, &localNodes);
            } else {
                // Next moves: search with null window
                int32_t nullAlpha = 0, nullBeta = 0;
                rootNullWindow(usIsWhite, alpha, beta, nullAlpha, nullBeta);
                
                score = this->searchRootMoveScore(rootBoard, m, nullAlpha, nullBeta, currPly, true, true, true, &localNodes);
                
                // PVS re-search: if null-window fails, re-search with full window
                // White: re-search if score > alpha (null window failed high)
                // Black: re-search if score < beta (null window failed low)
                const bool shouldResearch = shouldResearchPVS(score, alpha, beta, usIsWhite);
                if (shouldResearch) {
                    score = this->searchRootMoveScore(rootBoard, m, alpha, beta, currPly, true, true, true, &localNodes);
                }
            }

            searchedAnyMove = true;
            if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                break;
            }

            // Update best move and alpha-beta bounds
            this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);

            // Beta cutoff check after updateMinMax
            if (isBetaCutoff(bestScore, alpha, beta, usIsWhite)) break;
        }
        this->nodesSearched += localNodes;
        if (searchedAnyMove) this->eval = bestScore;
        return bestMove;
    }

    // --- YBWC Parallel ---
    // First move: full-window search
    {
        const auto& firstMove = rootMoves[0];
        const int32_t score = this->searchRootMoveScore(rootBoard, firstMove, alpha, beta, currPly, true, true, true, &localNodes);
        searchedAnyMove = true;
        this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, firstMove);
    }

    if (this->searchInterrupted.load(std::memory_order_relaxed)) {
        this->nodesSearched += localNodes;
        if (searchedAnyMove) this->eval = bestScore;
        return bestMove;
    }

    if (rootMoves.size <= 1) [[unlikely]] {
        this->nodesSearched += localNodes;
        if (searchedAnyMove) this->eval = bestScore;
        return bestMove;
    }

    // All threads use the same null-window snapshot for deterministic parallel screening.
    const int32_t sharedAlpha = alpha;
    const int32_t sharedBeta = beta;
    int32_t nullAlpha = 0;
    int32_t nullBeta = 0;
    rootNullWindow(usIsWhite, sharedAlpha, sharedBeta, nullAlpha, nullBeta);

    std::array<int32_t, MAX_MOVES> threadScores;
    threadScores.fill(Engine::initialBest(usIsWhite));
    std::array<uint64_t, MAX_MOVES> threadNodeCounts {};
    std::array<uint8_t, MAX_MOVES> threadNeedsResearch {};

    // Task-based root parallelism (work-stealing, better load balance)
    // Bound the number of threads to MAX_THREADS and the number of moves
    int candidateThreads = std::max(1, static_cast<int>(rootMoves.size) - 1);
    const int threadsToUse = std::min(this->MAX_THREADS, candidateThreads);

    if (threadsToUse <= 1) {
        for (int i = 1; i < rootMoves.size; ++i) {
            if (this->shouldAbortSearch()) {
                this->searchInterrupted.store(true, std::memory_order_relaxed);
                break;
            }

            chess::Board threadBoard = rootBoard;
            const auto m = rootMoves[i];
            uint64_t workerNodes = 0;
            const int32_t score = this->searchRootMoveScore(
                threadBoard, m, nullAlpha, nullBeta, currPly, true, false, false, &workerNodes);

            threadScores[i] = score;
            threadNodeCounts[i] = workerNodes;
            threadNeedsResearch[i] = static_cast<uint8_t>(shouldResearchPVS(score, sharedAlpha, sharedBeta, usIsWhite));
            if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                break;
            }
        }
    } else {
        // Parallel region with chunked tasks to reduce task overhead and copies
        // We create tasks that process a contiguous range of moves. Each task copies the board ONCE
        // and evaluates all moves in the chunk sequentially. We use taskgroup to
        // wait for all tasks deterministically before merging results.
        const int totalJobs = static_cast<int>(rootMoves.size - 1);
        int estimatedChunk = std::max(1, totalJobs / (threadsToUse * 4));
        const int chunk = std::min(16, estimatedChunk); // cap chunk size to avoid too-large tasks

        #pragma omp parallel num_threads(threadsToUse)        
        {
            #pragma omp single nowait
            {
                #pragma omp taskgroup
                {
                    for (int start = 1; start <= totalJobs; start += chunk) {
                        const int end = std::min(start + chunk, static_cast<int>(rootMoves.size));
                        #pragma omp task firstprivate(start, end)
                        {
                            if (!this->shouldAbortSearch()) {
                                chess::Board threadBoard = rootBoard;

                                for (int i = start; i < end; ++i) {
                                    if (this->shouldAbortSearch()) {
                                        break;
                                    }
                                    const auto m = rootMoves[i]; // local copy
                                    uint64_t workerNodes = 0;
                                    const int32_t score = this->searchRootMoveScore(
                                        threadBoard, m, nullAlpha, nullBeta, currPly, true, false, false, &workerNodes);

                                    threadScores[i] = score;
                                    threadNodeCounts[i] = workerNodes;
                                    threadNeedsResearch[i] = static_cast<uint8_t>(shouldResearchPVS(score, sharedAlpha, sharedBeta, usIsWhite));
                                    if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                                        break;
                                    }
                                }
                            }
                        } // task
                    }
                } // taskgroup
            } // single
        } // parallel
    }

    // Merge results deterministically (sequential order, no race)
    for (int i = 1; i < rootMoves.size; ++i) {
        if (this->searchInterrupted.load(std::memory_order_relaxed)) {
            break;
        }
        if (threadNodeCounts[i] == 0) continue;
        const auto& m = rootMoves[i];

        int32_t score = threadScores[i];
        if (threadNeedsResearch[i] != 0U) {
            uint64_t researchNodes = 0;
            score = this->searchRootMoveScore(rootBoard, m, alpha, beta, currPly, true, true, true, &researchNodes);
            localNodes += researchNodes;

            if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                break;
            }
        }

        this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);
        searchedAnyMove = true;

        if (isBetaCutoff(bestScore, alpha, beta, usIsWhite)) {
            break;
        }
    }

    localNodes = std::accumulate(threadNodeCounts.begin() + 1, threadNodeCounts.end(), localNodes);
    this->nodesSearched += localNodes;
    if (searchedAnyMove) this->eval = bestScore;
    return bestMove;
}

void Engine::storeRootHashMove(const chess::Board& rootBoard, const chess::Board::Move& move, uint64_t depth, int32_t score, uint8_t flag) noexcept {
    if (!chess::Coords::isInBounds(move.from) || !chess::Coords::isInBounds(move.to)) {
        return;
    }

    if (flag != tt::TranspositionTable::Entry::EXACT
        && flag != tt::TranspositionTable::Entry::LOWERBOUND
        && flag != tt::TranspositionTable::Entry::UPPERBOUND) {
        flag = tt::TranspositionTable::Entry::EXACT;
    }

    const uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(
        move.from.index, move.to.index, move.promotionPiece);
    this->tt.store(rootBoard.getHash(), static_cast<uint8_t>(depth), clampToTTScore(score), flag, encodedMove);
}

Engine::IterativeSearchResult Engine::runIterativeDeepening(chess::Board& rootBoard, uint64_t startDepth, uint64_t targetDepth, bool allowStop) noexcept {
    IterativeSearchResult result;
    const uint64_t firstDepth = std::max<uint64_t>(1, startDepth);
    const uint64_t maxDepth = std::max<uint64_t>(firstDepth, targetDepth);
    result.startDepth = firstDepth;
    result.targetDepth = maxDepth;

    MoveList<chess::Board::Move> moves = Engine::generateLegalMoves(rootBoard);
    if (moves.is_empty()) {
        const uint8_t toMove = rootBoard.getActiveColor();
        if (rootBoard.kings_bb[0] == 0) {
            result.bestScore = NEG_INF;
        } else if (rootBoard.kings_bb[1] == 0) {
            result.bestScore = POS_INF;
        } else if (rootBoard.isCheckmate(toMove)) {
            result.bestScore = (toMove == chess::Board::WHITE) ? NEG_INF : POS_INF;
        } else if (rootBoard.isDraw(toMove)) {
            result.bestScore = 0;
        } else {
            result.bestScore = this->evaluate(rootBoard);
        }
        this->eval = result.bestScore;
        return result;
    }

    result.hasLegalMoves = true;
    uint64_t interruptedDepth = 0;
    const bool searchBestMoveForWhite = (rootBoard.getActiveColor() == chess::Board::WHITE);
    chess::Board::Move bestMove = moves[0];
    int32_t prevPrevScore = 0;
    int32_t prevScore = 0;
    bool hasPrevScore = false;
    bool hasPrevPrevScore = false;
    constexpr int32_t MATE_SCORE_THRESHOLD = POS_INF - 2048;
    auto absScore = [](int32_t v) noexcept -> int32_t {
        if (v == std::numeric_limits<int32_t>::min()) return std::numeric_limits<int32_t>::max();
        return (v >= 0) ? v : -v;
    };
    
    for (uint64_t currentDepth = firstDepth; currentDepth <= maxDepth; ++currentDepth) {
        if (this->shouldAbortSearch()) {
            interruptedDepth = currentDepth;
            break;
        }

        this->depth = currentDepth;
        if (allowStop) {
            this->ponderCurrentDepth.store(currentDepth, std::memory_order_relaxed);
#ifdef DEBUG
            if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
                std::cout << "[PONDER] current depth: " << currentDepth << "\n";
            }
#endif
        }
        
        // Move ordering: bring previous iteration's best move to the front
        if (result.completedAnyDepth) {
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i] == bestMove) {
                    chess::Board::Move::rotate(moves, i);
                    break;
                }
            }
        }

        this->searchInterrupted.store(false, std::memory_order_relaxed);
        bool iterationCompleted = true;
        int32_t iterationAlpha = NEG_INF;
        int32_t iterationBeta = POS_INF;
        chess::Board::Move candidateBestMove = moves[0];

        const bool canUseAspiration =
            hasPrevScore
            && hasPrevPrevScore
            && result.completedAnyDepth
            && currentDepth >= 5
            && absScore(prevScore) < MATE_SCORE_THRESHOLD
            && absScore(prevPrevScore) < MATE_SCORE_THRESHOLD;

        if (!canUseAspiration) {
            candidateBestMove = this->getBestMove(rootBoard, moves, searchBestMoveForWhite);
            if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                iterationCompleted = false;
            }
        } else {
            // Dynamic aspiration window based on recent score volatility.
            const int64_t scoreDiff64 = static_cast<int64_t>(prevScore) - static_cast<int64_t>(prevPrevScore);
            const int64_t scoreSwing64 = (scoreDiff64 >= 0) ? scoreDiff64 : -scoreDiff64;
            const int32_t scoreSwing = static_cast<int32_t>(std::min<int64_t>(scoreSwing64, std::numeric_limits<int32_t>::max()));
            int32_t windowDelta = std::clamp<int32_t>(40 + (scoreSwing / 2), 60, 220);
            constexpr int32_t WINDOW_HARD_CAP = 1500;
            constexpr int MAX_ASP_RESEARCHES = 6;
            int aspirationResearches = 0;
            int32_t centerScore = prevScore;
            int32_t aspAlpha = saturatingSub32(centerScore, windowDelta);
            int32_t aspBeta = saturatingAdd32(centerScore, windowDelta);

            while (true) {
                iterationAlpha = aspAlpha;
                iterationBeta = aspBeta;
                candidateBestMove = this->getBestMove(rootBoard, moves, searchBestMoveForWhite, aspAlpha, aspBeta);
                if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                    iterationCompleted = false;
                    break;
                }

                const int32_t score = this->eval;
                const bool failLow = (score <= aspAlpha);
                const bool failHigh = (score >= aspBeta);
                if (!failLow && !failHigh) {
                    break;
                }

                ++aspirationResearches;
                ++result.aspirationResearches;
                if (failLow) {
                    ++result.aspirationFailLow;
                    centerScore = std::min(centerScore, score);
                } else {
                    ++result.aspirationFailHigh;
                    centerScore = std::max(centerScore, score);
                }

                windowDelta = std::min<int32_t>(WINDOW_HARD_CAP, windowDelta * 2 + 20);
                if (aspirationResearches >= MAX_ASP_RESEARCHES || windowDelta >= WINDOW_HARD_CAP) {
                    iterationAlpha = NEG_INF;
                    iterationBeta = POS_INF;
                    candidateBestMove = this->getBestMove(rootBoard, moves, searchBestMoveForWhite);
                    if (this->searchInterrupted.load(std::memory_order_relaxed)) {
                        iterationCompleted = false;
                    }
                    break;
                }

                if (failLow) {
                    aspAlpha = std::max<int32_t>(NEG_INF, saturatingSub32(centerScore, windowDelta));
                    aspBeta = std::min<int32_t>(POS_INF, saturatingAdd32(centerScore, std::max<int32_t>(40, windowDelta / 2)));
                } else {
                    aspAlpha = std::max<int32_t>(NEG_INF, saturatingSub32(centerScore, std::max<int32_t>(40, windowDelta / 2)));
                    aspBeta = std::min<int32_t>(POS_INF, saturatingAdd32(centerScore, windowDelta));
                }
            }
        }

        if (!iterationCompleted) {
            interruptedDepth = currentDepth;
            break;
        }

        if (hasPrevScore) {
            prevPrevScore = prevScore;
            hasPrevPrevScore = true;
        }

        bestMove = candidateBestMove;
        prevScore = this->eval;
        hasPrevScore = true;
        result.completedAnyDepth = true;
        ++result.completedIterations;
        result.completedDepth = currentDepth;
        if ((currentDepth & 1ULL) == 0ULL) {
            result.completedEvenDepth = currentDepth;
        }
        result.bestMove = bestMove;
        result.bestScore = this->eval;
        result.rootScoreBound = tt::determineFlag(result.bestScore, iterationAlpha, iterationBeta);
        this->storeRootHashMove(
            rootBoard,
            bestMove,
            currentDepth,
            this->eval,
            static_cast<uint8_t>(result.rootScoreBound));

        if (allowStop) {
            this->ponderLastCompletedDepth.store(currentDepth, std::memory_order_relaxed);
            if ((currentDepth & 1ULL) == 0ULL) {
                this->ponderLastCompletedEvenDepth.store(currentDepth, std::memory_order_relaxed);
            }
            this->ponderAspirationResearches.store(result.aspirationResearches, std::memory_order_relaxed);
            this->ponderAspirationFailLow.store(result.aspirationFailLow, std::memory_order_relaxed);
            this->ponderAspirationFailHigh.store(result.aspirationFailHigh, std::memory_order_relaxed);
#ifdef DEBUG
            if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
                std::cout << "[PONDER] last completed depth: " << currentDepth
                          << " (last even: " << result.completedEvenDepth
                          << ", asp retries: " << result.aspirationResearches
                          << ", fail-low: " << result.aspirationFailLow
                          << ", fail-high: " << result.aspirationFailHigh << ")\n";
            }
#endif
        }
    }

    result.interruptedDepth = interruptedDepth;
    if (allowStop) {
        this->ponderInterruptedDepth.store(interruptedDepth, std::memory_order_relaxed);
        this->ponderAspirationResearches.store(result.aspirationResearches, std::memory_order_relaxed);
        this->ponderAspirationFailLow.store(result.aspirationFailLow, std::memory_order_relaxed);
        this->ponderAspirationFailHigh.store(result.aspirationFailHigh, std::memory_order_relaxed);
    }

    return result;
}

void Engine::ponderLoop(chess::Board rootBoard) noexcept {
    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);
    this->nodesSearched = 0;
    this->tt.incrementGeneration();
    this->ponderCurrentDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedEvenDepth.store(0, std::memory_order_relaxed);
    this->ponderInterruptedDepth.store(0, std::memory_order_relaxed);
    this->ponderAspirationResearches.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailLow.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailHigh.store(0, std::memory_order_relaxed);

    if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
#ifdef DEBUG
        std::cout << "[PONDER] started from depth " << Engine::DEFAULTDEPTH << "\n";
#endif
    }

    // Keep extending depth while opponent is thinking: 10, 11, 12, ...
    #ifdef DEBUG
    const IterativeSearchResult ponderResult = this->runIterativeDeepening(
        rootBoard,
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        static_cast<uint64_t>(Engine::MAX_PLY),
        true);

    if (this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
        std::cout << "[PONDER] ended. current depth: " << this->getPonderCurrentDepth()
                  << ", last completed depth: " << this->getPonderLastCompletedDepth()
                  << ", last even depth: " << this->ponderLastCompletedEvenDepth.load(std::memory_order_relaxed)
                  << ", interrupted depth: " << this->getPonderInterruptedDepth()
                  << ", asp retries: " << ponderResult.aspirationResearches
                  << ", fail-low: " << ponderResult.aspirationFailLow
                  << ", fail-high: " << ponderResult.aspirationFailHigh << "\n";
    }
    #else
    this->runIterativeDeepening(
        rootBoard,
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        static_cast<uint64_t>(Engine::MAX_PLY),
        true);
    #endif

    this->ponderingActive.store(false, std::memory_order_release);
}

void Engine::startPondering() noexcept {
    if (this->isGameOver()) return;

    this->stopPondering();

    const chess::Board rootBoard = this->board;
    this->ponderingStopRequested.store(false, std::memory_order_release);
    this->stopSearchRequested.store(false, std::memory_order_release);
    this->searchInterrupted.store(false, std::memory_order_release);
    this->ponderingActive.store(true, std::memory_order_release);

    try {
        this->ponderingThread = std::thread([this, rootBoard]() mutable {
            this->ponderLoop(rootBoard);
        });
    } catch (...) {
        this->ponderingActive.store(false, std::memory_order_release);
        this->ponderingStopRequested.store(false, std::memory_order_release);
        this->stopSearchRequested.store(false, std::memory_order_release);
    }
}

void Engine::stopPondering() noexcept {
    const bool hadActivePonder = this->ponderingActive.load(std::memory_order_relaxed)
        || this->ponderingThread.joinable();

    this->ponderingStopRequested.store(true, std::memory_order_release);
    this->stopSearchRequested.store(true, std::memory_order_release);

    if (this->ponderingThread.joinable()) {
        this->ponderingThread.join();
    }

    this->ponderingActive.store(false, std::memory_order_release);
    this->ponderingStopRequested.store(false, std::memory_order_release);
    this->stopSearchRequested.store(false, std::memory_order_release);
    this->searchInterrupted.store(false, std::memory_order_release);

    if (hadActivePonder && this->ponderDebugEnabled.load(std::memory_order_relaxed)) {
#ifdef DEBUG
        std::cout << "[PONDER] stop requested. current depth: " << this->getPonderCurrentDepth()
                  << ", last completed depth: " << this->getPonderLastCompletedDepth()
                  << ", last even depth: " << this->ponderLastCompletedEvenDepth.load(std::memory_order_relaxed)
                  << ", interrupted depth: " << this->getPonderInterruptedDepth()
                  << ", asp retries: " << this->ponderAspirationResearches.load(std::memory_order_relaxed)
                  << ", fail-low: " << this->ponderAspirationFailLow.load(std::memory_order_relaxed)
                  << ", fail-high: " << this->ponderAspirationFailHigh.load(std::memory_order_relaxed) << "\n";
#endif
    }
}

void Engine::stopThinking() noexcept {
    this->stopPondering();
}

// =========================================================================
// UCI-SAFE SEARCH: Find best move WITHOUT modifying engine board state.
// =========================================================================
// In UCI protocol, the GUI owns board state. The engine must NOT apply the
// best move on this->board, must NOT update gameResult, must NOT start
// pondering. The GUI will send a new "position" command before the next "go".
//
// This avoids:
// 1. Pondering polluting TT/history with entries from the wrong position
// 2. Board state desynchronization between engine and GUI
// 3. Non-deterministic move selection caused by stale pondering data
chess::Board::Move Engine::searchUCI(uint64_t requestedDepth) noexcept {
    this->stopPondering();

    const uint64_t targetDepth = std::max<uint64_t>(
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        requestedDepth);
    if (targetDepth == 0) return chess::Board::Move{};

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);

    this->tt.incrementGeneration();
    this->nodesSearched = 0;

    // History soft reset (same as search())
    int16_t* historyFlat = &this->history[0][0][0];
    constexpr int HISTORY_CELLS = 2 * 64 * 64;
    #pragma omp simd
    for (int i = 0; i < HISTORY_CELLS; ++i) {
        historyFlat[i] >>= 1;
    }

    // Search on a COPY of the board to avoid mutating this->board
    chess::Board searchBoard = this->board;
    IterativeSearchResult result = this->runIterativeDeepening(searchBoard, 1, targetDepth, false);
    this->depth = targetDepth;

    if (!result.hasLegalMoves || !result.completedAnyDepth) {
        MoveList<chess::Board::Move> fallbackMoves = Engine::generateLegalMoves(this->board);
        if (fallbackMoves.is_empty()) {
            return chess::Board::Move{};
        }
        this->bestMove = fallbackMoves[0];
        this->eval = this->evaluate(this->board);
        return this->bestMove;
    }

    this->bestMove = result.bestMove;
    this->eval = result.bestScore;

    // Do NOT apply the move on this->board.
    // Do NOT call updateGameResult().
    // Do NOT start pondering.
    return this->bestMove;
}

void Engine::search(uint64_t requestedDepth) noexcept {
    this->stopPondering();

    const uint64_t targetDepth = std::max<uint64_t>(
        static_cast<uint64_t>(Engine::DEFAULTDEPTH),
        requestedDepth);
    if (targetDepth == 0) return;

    this->stopSearchRequested.store(false, std::memory_order_relaxed);
    this->searchInterrupted.store(false, std::memory_order_relaxed);

    // Increment TT generation to age old entries from previous searches
    this->tt.incrementGeneration();

    // Reset the nodes searched counter
    this->nodesSearched = 0; 

    // ===================================================
    // HISTORY TABLE SOFT RESET - Age-based decay
    // ===================================================
    // Prevent stale data from dominating move ordering
    // Divide all history values by 2 at the start of each new search
    // This gives recent data more weight while preserving good moves
    int16_t* historyFlat = &this->history[0][0][0];
    constexpr int HISTORY_CELLS = 2 * 64 * 64;
    #pragma omp simd
    for (int i = 0; i < HISTORY_CELLS; ++i) {
        historyFlat[i] >>= 1; // Divide by 2
    }

    IterativeSearchResult result = this->runIterativeDeepening(this->board, 1, targetDepth, false);
    this->depth = targetDepth;

    if (!result.hasLegalMoves) {
        this->updateGameResult();
        return;
    }

    if (!result.completedAnyDepth) {
        MoveList<chess::Board::Move> fallbackMoves = Engine::generateLegalMoves(this->board);
        if (fallbackMoves.is_empty()) {
            this->updateGameResult();
            return;
        }
        result.bestMove = fallbackMoves[0];
        result.bestScore = this->evaluate(this->board);
        this->eval = result.bestScore;
    }

    const chess::Board::Move bestMove = result.bestMove;

    (void)this->board.move(bestMove.from, bestMove.to, 
        isPromotionMove(this->board, bestMove) ? (bestMove.promotionPiece != '\0' ? bestMove.promotionPiece : 'q') : '\0');
    this->updateGameResult();
    this->bestMove = bestMove;
    this->eval = result.bestScore;

    this->appendMoveHistoryEntry(bestMove.from, bestMove.to, bestMove.promotionPiece);

    if (!this->isGameOver()) {
        this->startPondering();
    }

#ifdef DEBUG
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    if (bestMove.promotionPiece != '\0') {
        moveStr += bestMove.promotionPiece;
    }
    std::cout << "Engine plays: " << moveStr << " (score: " << this->eval << ")\n";
    // std::cout << "[DEBUG] TT probes: " << ttProbes << ", hits: " << ttHits << "\n";

#endif
}

} // namespace engine
