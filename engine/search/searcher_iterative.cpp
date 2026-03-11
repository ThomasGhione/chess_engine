#include "searcher.hpp"
#include "../eval/evaluator.hpp"
#include "../movegen/movegen.hpp"

namespace engine {

SearchResult Searcher::runIterativeDeepening(chess::Board& rootBoard, uint64_t startDepth, uint64_t targetDepth) noexcept {
    SearchResult result;
    const uint64_t firstDepth = std::max<uint64_t>(1, startDepth);
    const uint64_t maxDepth = std::max<uint64_t>(firstDepth, targetDepth);
    result.depth = static_cast<uint8_t>(maxDepth);

    // Generate legal moves at root
    ChessMoveList moves = MoveGenerator::generateLegalMoves(rootBoard);
    if (moves.is_empty()) {
        const uint8_t toMove = rootBoard.getActiveColor();
        if (rootBoard.kings_bb[0] == 0) {
            result.score = NEG_INF;
        } else if (rootBoard.kings_bb[1] == 0) {
            result.score = POS_INF;
        } else if (rootBoard.isCheckmate(toMove)) {
            result.score = (toMove == chess::Board::WHITE) ? NEG_INF : POS_INF;
        } else if (rootBoard.isDraw(toMove)) {
            result.score = 0;
        } else {
            result.score = Evaluator::evaluate(rootBoard);
        }
        result.nodesSearched = state_.nodesSearched;
        return result;
    }

    // Initial setup
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

    // Iterative deepening loop
    for (uint64_t currentDepth = firstDepth; currentDepth <= maxDepth; ++currentDepth) {
        if (shouldAbort()) {
            interruptedDepth = currentDepth;
            state_.interrupted.store(true, std::memory_order_relaxed);
            break;
        }

        // Move ordering: bring previous iteration's best move to the front
        if (result.nodesSearched > 0) { // Check if we've completed at least one iteration
            for (int i = 0; i < moves.size; ++i) {
                if (moves[i].from.index == bestMove.from.index && 
                    moves[i].to.index == bestMove.to.index &&
                    moves[i].promotionPiece == bestMove.promotionPiece) {
                    chess::Board::Move::rotate(moves, i);
                    break;
                }
            }
        }

        bool iterationCompleted = true;
        int32_t iterationAlpha = NEG_INF;
        int32_t iterationBeta = POS_INF;
        chess::Board::Move candidateBestMove = moves[0];

        const bool canUseAspiration =
            hasPrevScore
            && hasPrevPrevScore
            && (state_.nodesSearched > 0)
            && currentDepth >= 5
            && absScore(prevScore) < MATE_SCORE_THRESHOLD
            && absScore(prevPrevScore) < MATE_SCORE_THRESHOLD;

        if (!canUseAspiration) {
            // Search without aspiration window
            int32_t bestScore = Engine::initialBest(searchBestMoveForWhite);
            candidateBestMove = moves[0];
            constexpr int currPly = 1;
            uint64_t localNodes = 0;

            // Sort root moves
            {
                const uint64_t rootHash = rootBoard.getHash();
                (void)sortLegalMoves(moves, 0, rootBoard, searchBestMoveForWhite, rootHash, nullptr);
            }

            // Sequential search at root with PVS
            for (int i = 0; i < moves.size; ++i) {
                if (shouldAbort()) {
                    state_.interrupted.store(true, std::memory_order_relaxed);
                    iterationCompleted = false;
                    break;
                }

                const auto& m = moves[i];
                int32_t score = 0;
                
                // Execute root move and search
                chess::Board::MoveState moveState;
                doMoveWithPromotion(rootBoard, m, moveState);
                
                if (i == 0) {
                    // First move: full window
                    score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, NEG_INF, POS_INF, 
                                 currPly, &m, &localNodes);
                } else {
                    // Null window
                    int32_t nullAlpha = searchBestMoveForWhite ? NEG_INF : saturatingSub32(POS_INF, 1);
                    int32_t nullBeta = searchBestMoveForWhite ? saturatingAdd32(NEG_INF, 1) : POS_INF;
                    
                    score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, nullAlpha, nullBeta,
                                 currPly, &m, &localNodes);
                    
                    // PVS re-search if needed
                    const bool shouldResearch = 
                        (searchBestMoveForWhite && score > NEG_INF) ||
                        (!searchBestMoveForWhite && score < POS_INF);
                    
                    if (shouldResearch) {
                        score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, NEG_INF, POS_INF,
                                     currPly, &m, &localNodes);
                    }
                }
                
                rootBoard.undoMove(m, moveState);
                
                if (state_.interrupted.load(std::memory_order_relaxed)) {
                    iterationCompleted = false;
                    break;
                }
                
                // Update best move and bounds
                const bool isBetter = searchBestMoveForWhite ? (score > bestScore) : (score < bestScore);
                if (isBetter) {
                    bestScore = score;
                    candidateBestMove = m;
                }
                
                // Update alpha/beta for next move
                if (searchBestMoveForWhite) {
                    if (score > iterationAlpha) iterationAlpha = score;
                } else {
                    if (score < iterationBeta) iterationBeta = score;
                }
            }
            
            state_.nodesSearched += localNodes;
            
            if (!iterationCompleted) {
                break;
            }
        } else {
            // Search with aspiration window
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
                
                // Do search with aspiration window
                int32_t bestScore = Engine::initialBest(searchBestMoveForWhite);
                candidateBestMove = moves[0];
                constexpr int currPly = 1;
                uint64_t localNodes = 0;

                // Sort root moves
                {
                    const uint64_t rootHash = rootBoard.getHash();
                    (void)sortLegalMoves(moves, 0, rootBoard, searchBestMoveForWhite, rootHash, nullptr);
                }

                // Search moves with aspiration window
                for (int i = 0; i < moves.size; ++i) {
                    if (shouldAbort()) {
                        state_.interrupted.store(true, std::memory_order_relaxed);
                        iterationCompleted = false;
                        break;
                    }

                    const auto& m = moves[i];
                    int32_t score = 0;
                    
                    chess::Board::MoveState moveState;
                    doMoveWithPromotion(rootBoard, m, moveState);
                    
                    if (i == 0) {
                        score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, aspAlpha, aspBeta,
                                     currPly, &m, &localNodes);
                    } else {
                        int32_t nullAlpha = searchBestMoveForWhite ? aspAlpha : saturatingSub32(aspBeta, 1);
                        int32_t nullBeta = searchBestMoveForWhite ? saturatingAdd32(aspAlpha, 1) : aspBeta;
                        
                        score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, nullAlpha, nullBeta,
                                     currPly, &m, &localNodes);
                        
                        const bool shouldResearch = 
                            (searchBestMoveForWhite && score > aspAlpha) ||
                            (!searchBestMoveForWhite && score < aspBeta);
                        
                        if (shouldResearch) {
                            score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, aspAlpha, aspBeta,
                                         currPly, &m, &localNodes);
                        }
                    }
                    
                    rootBoard.undoMove(m, moveState);
                    
                    if (state_.interrupted.load(std::memory_order_relaxed)) {
                        iterationCompleted = false;
                        break;
                    }

                    const bool isBetter = searchBestMoveForWhite ? (score > bestScore) : (score < bestScore);
                    if (isBetter) {
                        bestScore = score;
                        candidateBestMove = m;
                    }
                    
                    if (searchBestMoveForWhite) {
                        if (score > iterationAlpha) iterationAlpha = score;
                    } else {
                        if (score < iterationBeta) iterationBeta = score;
                    }
                }
                
                state_.nodesSearched += localNodes;
                
                if (!iterationCompleted) {
                    break;
                }

                const int32_t score = bestScore;
                const bool failLow = searchBestMoveForWhite ? (score <= aspAlpha) : (score >= aspBeta);
                const bool failHigh = searchBestMoveForWhite ? (score >= aspBeta) : (score <= aspAlpha);
                
                if (!failLow && !failHigh) {
                    break; // Window was accurate, no re-search needed
                }

                ++aspirationResearches;
                if (failLow) {
                    centerScore = searchBestMoveForWhite ? std::min(centerScore, score) : std::max(centerScore, score);
                } else {
                    centerScore = searchBestMoveForWhite ? std::max(centerScore, score) : std::min(centerScore, score);
                }

                windowDelta = std::min<int32_t>(WINDOW_HARD_CAP, windowDelta * 2 + 20);
                if (aspirationResearches >= MAX_ASP_RESEARCHES || windowDelta >= WINDOW_HARD_CAP) {
                    // Do one final search with full window
                    int32_t bestScore = Engine::initialBest(searchBestMoveForWhite);
                    candidateBestMove = moves[0];
                    uint64_t finalLocalNodes = 0;

                    {
                        const uint64_t rootHash = rootBoard.getHash();
                        (void)sortLegalMoves(moves, 0, rootBoard, searchBestMoveForWhite, rootHash, nullptr);
                    }

                    for (int i = 0; i < moves.size; ++i) {
                        if (shouldAbort()) {
                            state_.interrupted.store(true, std::memory_order_relaxed);
                            iterationCompleted = false;
                            break;
                        }

                        const auto& m = moves[i];
                        int32_t score = 0;
                        
                        chess::Board::MoveState moveState;
                        doMoveWithPromotion(rootBoard, m, moveState);
                        
                        if (i == 0) {
                            score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, NEG_INF, POS_INF,
                                         currPly, &m, &finalLocalNodes);
                        } else {
                            int32_t nullAlpha = searchBestMoveForWhite ? NEG_INF : saturatingSub32(POS_INF, 1);
                            int32_t nullBeta = searchBestMoveForWhite ? saturatingAdd32(NEG_INF, 1) : POS_INF;
                            
                            score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, nullAlpha, nullBeta,
                                         currPly, &m, &finalLocalNodes);
                            
                            const bool shouldResearch = 
                                (searchBestMoveForWhite && score > NEG_INF) ||
                                (!searchBestMoveForWhite && score < POS_INF);
                            
                            if (shouldResearch) {
                                score = search(rootBoard, static_cast<int32_t>(currentDepth) - 1, NEG_INF, POS_INF,
                                             currPly, &m, &finalLocalNodes);
                            }
                        }
                        
                        rootBoard.undoMove(m, moveState);
                        
                        if (state_.interrupted.load(std::memory_order_relaxed)) {
                            iterationCompleted = false;
                            break;
                        }

                        const bool isBetter = searchBestMoveForWhite ? (score > bestScore) : (score < bestScore);
                        if (isBetter) {
                            bestScore = score;
                            candidateBestMove = m;
                        }
                    }
                    
                    state_.nodesSearched += finalLocalNodes;
                    iterationAlpha = NEG_INF;
                    iterationBeta = POS_INF;
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

        // Update iteration results
        if (hasPrevScore) {
            prevPrevScore = prevScore;
            hasPrevPrevScore = true;
        }

        bestMove = candidateBestMove;
        prevScore = Evaluator::evaluate(rootBoard); // Use evaluator to get score
        hasPrevScore = true;
        result.depth = static_cast<uint8_t>(currentDepth);
        result.bestMove = bestMove;
        result.score = prevScore;
        
        // Convert TT flag to SearchResult bound type
        auto ttFlag = tt::determineFlag(prevScore, iterationAlpha, iterationBeta);
        if (ttFlag == tt::TranspositionTable::Entry::Flag::EXACT) {
            result.bound = SearchResult::BoundType::EXACT;
        } else if (ttFlag == tt::TranspositionTable::Entry::Flag::LOWERBOUND) {
            result.bound = SearchResult::BoundType::LOWERBOUND;
        } else {
            result.bound = SearchResult::BoundType::UPPERBOUND;
        }
    }

    result.interrupted = (interruptedDepth > 0);
    result.nodesSearched = state_.nodesSearched;

    return result;
}

} // namespace engine
