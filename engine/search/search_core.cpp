#include "../movegen/movegen.hpp"
#include "../engine.hpp"
#include "../../tt/ttentry.hpp"
#include "searcher.hpp"
#include <cmath>

namespace engine {

namespace {
inline int32_t saturatingAdd32Core(int32_t lhs, int32_t rhs) noexcept {
    const int64_t sum = static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs);
    if (sum > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) return std::numeric_limits<int32_t>::max();
    if (sum < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(sum);
}

inline int32_t saturatingSub32Core(int32_t lhs, int32_t rhs) noexcept {
    const int64_t diff = static_cast<int64_t>(lhs) - static_cast<int64_t>(rhs);
    if (diff > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) return std::numeric_limits<int32_t>::max();
    if (diff < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(diff);
}

inline int16_t clampHeuristic16Core(int32_t value) noexcept {
    constexpr int32_t MIN_I16 = -32768;
    constexpr int32_t MAX_I16 = 32767;
    return static_cast<int16_t>(std::clamp(value, MIN_I16, MAX_I16));
}
} // namespace

int32_t Engine::stalemateScoreFromMaterialDelta(int32_t matDelta) noexcept {
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) return 0;
    // Scale contempt for stalemate with material edge, but keep it bounded.
    const int32_t advantage = std::abs(matDelta);
    const int32_t scaledPenalty =
        STALEMATE_DRAW_PENALTY_MINOR + (advantage - STALEMATE_MATERIAL_THRESHOLD) / 2;
    const int32_t stalematePenalty = std::clamp<int32_t>(
        scaledPenalty, STALEMATE_DRAW_PENALTY_MINOR, STALEMATE_DRAW_PENALTY_MAJOR);
    return (matDelta > 0) ? -stalematePenalty : stalematePenalty;
}

int32_t Engine::repetitionDrawScore(const chess::Board& b) noexcept {
    const int32_t matDelta = getMaterialDelta(b);
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) {
        return 0;
    }

    const int32_t contempt = std::min(static_cast<int32_t>(200), std::abs(matDelta) / 2);
    return (matDelta > 0) ? -contempt : contempt;
}

bool Engine::hasInsufficientMaterialDraw(const chess::Board& b) noexcept {
    //FIXME: Fare chiamate dirette! E non usare 0 e 1 ma WHITE E BLACK accedendo a BOARD
    const uint64_t wKnights = b.knights_bb[0];
    const uint64_t bKnights = b.knights_bb[1];
    const uint64_t wBishops = b.bishops_bb[0];
    const uint64_t bBishops = b.bishops_bb[1];
    const uint64_t wMinors = wKnights | wBishops;
    const uint64_t bMinors = bKnights | bBishops;

    if (wMinors == 0ULL && bMinors == 0ULL) {
        return true;
    }

    const int wMinorCount = __builtin_popcountll(wMinors);
    const int bMinorCount = __builtin_popcountll(bMinors);
    return (wMinorCount <= 1 && bMinorCount == 0)
        || (bMinorCount <= 1 && wMinorCount == 0);
}

// Helper to handle terminal nodes and transposition table lookups
bool Engine::handleSearchPrelude(const int32_t& depth, const AlphaBeta& bounds, int32_t& score, uint64_t hashKey) noexcept {
    // Transposition table lookup (hashKey already computed by caller to avoid duplication)
    if (depth >= 2) this->tt.prefetch(hashKey);
    
    //FIXME: Fare una dichiarazione su sola riga
    int32_t ttScore = 0;
    int32_t ttAlpha = 0;
    int32_t ttBeta = 0;
    toTTProbeBounds(bounds.alpha, bounds.beta, ttAlpha, ttBeta);
    if (this->tt.probe(hashKey, static_cast<uint8_t>(depth), ttAlpha, ttBeta, ttScore)) {
        score = static_cast<int32_t>(ttScore);
        return true;
    }
    return false;
}

bool Engine::tryNullMovePruning(chess::Board& b, const SearchNodeState& node,
                                int32_t depth, int32_t alpha, int32_t beta, int ply,
                                bool useTT, bool allowTTWrite, bool allowHeuristicUpdates,
                                uint64_t* nodeCounter, int32_t& outScore) noexcept {
    //FIXME: Eliminare numeri magici
    const int32_t reduction = 3 + depth / 8;

    chess::Board::MoveState nullState;
    b.doNullMove(nullState);

    const int32_t nullScore = this->searchPosition(
        b, depth - reduction, alpha, beta, ply + 1,
        useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter, false);

    b.undoNullMove(nullState);

    if (!isBetaCutoff(nullScore, alpha, beta, node.usIsWhite)) {
        return false;
    }

    bool confirmedCutoff = true;
    //FIXME: Eliminare numeri magici
    if (depth >= 10) {
        const int32_t verifyScore = this->searchPosition(
            b, depth - reduction, alpha, beta, ply,
            useTT, allowTTWrite, allowHeuristicUpdates, nullptr, nodeCounter, false);
        confirmedCutoff = isBetaCutoff(verifyScore, alpha, beta, node.usIsWhite);
    }

    if (!confirmedCutoff) {
        return false;
    }

    if (!b.hasAnyLegalMove(node.activeColor)) {
        outScore = stalemateScoreFromMaterialDelta(getMaterialDelta(b));
        return true;
    }

    outScore = cutoffValue(alpha, beta, node.usIsWhite);
    return true;
}

bool Engine::tryReverseFutilityPruning(chess::Board& b, const SearchNodeState& node,
                                       int32_t depth, int32_t alpha, int32_t beta, int ply,
                                       int32_t& outScore) noexcept {
    //FIXME: Portare fuori costante
    constexpr int32_t RFP_MARGIN_PER_DEPTH = 110;

    //FIXME: Eliminare numerici magici
    if (node.isPVNode || node.inCheck || node.isPawnEndgameForPruning || ply <= 0 || depth > 3) {
        return false;
    }

    const int32_t rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
    const int32_t rfpScore = node.usIsWhite
        ? (node.staticEval - rfpMargin)
        : (node.staticEval + rfpMargin);
    if (!isBetaCutoff(rfpScore, alpha, beta, node.usIsWhite)) {
        return false;
    }

    if (!b.hasAnyLegalMove(node.activeColor)) {
        outScore = stalemateScoreFromMaterialDelta(getMaterialDelta(b));
        return true;
    }

    outScore = node.staticEval;
    return true;
}

// Helper to search through all moves and find best move with its score
Engine::SearchMoveResult Engine::searchMoves(chess::Board& b, const MoveList<chess::Board::Move>& orderedMoves,
                                             bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds,
                                             bool useTT, bool allowHeuristicUpdates, bool allowTTWrite) noexcept {
    int32_t best = Engine::initialBest(usIsWhite);
    chess::Board::Move bestMove = orderedMoves[0];
    bool searchedAnyMove = false;

    // =========================================================================
    // HISTORY MALUS: Track quiet moves searched before cutoff
    // =========================================================================
    // When a beta cutoff occurs, penalize all quiet moves that were searched
    // but failed to produce a cutoff to improve move ordering over time.
    struct QuietEntry { uint8_t from; uint8_t to; };
    constexpr int MAX_QUIETS_TRACKED = 64;
    QuietEntry searchedQuiets[MAX_QUIETS_TRACKED];
    int numSearchedQuiets = 0;

    // Pre-compute endgame buckets once before the loop.
    const int nonPawnMajorsForLMR = __builtin_popcountll(b.knights_bb[0] | b.knights_bb[1] |
                                             b.bishops_bb[0] | b.bishops_bb[1] |
                                             b.rooks_bb[0]   | b.rooks_bb[1]   |
                                             b.queens_bb[0]  | b.queens_bb[1]);
    // Delicate endings (pure minor/pawn/king races): keep pruning conservative.
    //FIXME: Eliminare numerici magici
    const bool isDelicateEndgame = (nonPawnMajorsForLMR <= 2);
    // Broad ending bucket used for softer margins/thresholds.
    //FIXME: Eliminare numerici magici
    const bool isLateEndgame = (nonPawnMajorsForLMR <= 5);

    // =========================================================================
    // FUTILITY PRUNING margins (main search)
    // =========================================================================
    //FIXME: Eliminare numerici magici
    static constexpr int32_t FUTILITY_MARGINS_MG[] = {0, 260, 520}; // depth 0,1,2
    static constexpr int32_t FUTILITY_MARGINS_EG[] = {0, 170, 350}; // depth 0,1,2
    //FIXME: Eliminare numerici magici
    const bool canFutilityPrune = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;
    const int32_t futilityMargin = canFutilityPrune
        ? (isLateEndgame ? FUTILITY_MARGINS_EG[ctx.depth] : FUTILITY_MARGINS_MG[ctx.depth])
        : 0;

    // =========================================================================
    // LATE MOVE PRUNING thresholds
    // =========================================================================
    //FIXME: Eliminare numerici magici
    static constexpr int LMP_THRESHOLDS_MG[] = {0, 12, 20, 30}; // depth 0,1,2,3
    static constexpr int LMP_THRESHOLDS_EG[] = {0, 16, 26, 38}; // depth 0,1,2,3
    //FIXME: Eliminare numerici magici
    const bool canLMP = !ctx.isPVNode && !isDelicateEndgame && !ctx.inCheck && ctx.ply > 0 && ctx.depth <= 2 && ctx.depth >= 1;
    const int lmpThreshold = canLMP
        ? (isLateEndgame ? LMP_THRESHOLDS_EG[ctx.depth] : LMP_THRESHOLDS_MG[ctx.depth])
        : 999; // effectively disable LMP when not applicable

    const uint8_t oppColor = chess::Board::oppositeColor(ctx.activeColor);
    int moveIndex = 0;
    //FIXME: Trasformare in funzione helper
    for (const auto& m : orderedMoves) {
        if (this->shouldAbortSearch()) {
            this->searchInterrupted.store(true, std::memory_order_relaxed);
            break;
        }
        const bool isFirstMove = (moveIndex == 0);
        
        const bool wasCapture = (b.get(m.to) != chess::Board::EMPTY) || isEnPassantCapture(b, m);
        const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const bool isPromotionCandidate = (fromPieceType == chess::Board::PAWN) 
            && (m.to.rank() == chess::Board::promotionRank(usIsWhite));
        const bool isQuietMove = !wasCapture && !isPromotionCandidate;

        // =========================================================================
        // LATE MOVE PRUNING: Skip very late quiet moves at low depth
        // =========================================================================
	//FIXME: Trasformare codizione in funzione helper
        if (canLMP && isQuietMove && moveIndex >= lmpThreshold) {
            ++moveIndex;
            continue; // Completely skip this move
        }

        // =========================================================================
        // FUTILITY PRUNING: Skip quiet moves that can't improve the position
        // =========================================================================
	//FIXME: Trasformare codizione in funzione helper
        if (canFutilityPrune && isQuietMove && moveIndex > 0
            && shouldDeltaPrune(ctx.staticEval, futilityMargin, bounds.alpha, bounds.beta, usIsWhite)) {
            ++moveIndex;
            continue;
        }

        chess::Board::MoveState state;
        const bool isPromo = doMoveWithPromotion(b, m, state);

        // LMR: reduce depth for late, non-critical moves
        // LOGARITHMIC LMR: reduction = floor(log(depth) * log(moveIndex) / C)
        // NOTE: nonPawnMajors/isEndgame pre-computed BEFORE loop for correctness + speed
        const bool inConservativeEndgameLMR = isLateEndgame && !isDelicateEndgame;
        const int lmrMinMoveIndex = inConservativeEndgameLMR ? 14 : 12;
        const bool lmrStructuralCandidate = (ctx.depth > 6)
            && (moveIndex >= lmrMinMoveIndex)
            && !isPromo
            && (!wasCapture)
            && !isDelicateEndgame;

        const bool forcingCandidate = (wasCapture || isPromo || moveIndex < 3);
        const bool needsCheckInfo =
            (ctx.depth >= 2 && ctx.depth <= 4 && forcingCandidate) || lmrStructuralCandidate;
        const bool givesCheck = needsCheckInfo ? b.inCheck(oppColor) : false;

        // =========================================================================
        // CHECK EXTENSION (SELECTIVE, DETERMINISTIC)
        // =========================================================================
        // Avoid extending every checking move: that can stall depth reduction in
        // long checking sequences and hurt both speed and tactical stability.
        // Extend only forcing checks and only near the horizon.
        const bool isForcingCheck = givesCheck && forcingCandidate;
        const bool shouldCheckExtend = isForcingCheck && (ctx.depth >= 2) && (ctx.depth <= 4);
        const int32_t childDepth = ctx.depth - 1 + (shouldCheckExtend ? 1 : 0);
        const bool canReduce = lmrStructuralCandidate
            && !givesCheck
            && !this->isKillerMove(m, killerMoves, ctx.ply);

        // PVS windowing:
        // - First move: full window (PV candidate)
        // - Other moves: null window, then re-search full window only on fail-high/low
        const int32_t searchAlpha = isFirstMove ? bounds.alpha : (usIsWhite ? bounds.alpha : saturatingSub32Core(bounds.beta, 1));
        const int32_t searchBeta  = isFirstMove ? bounds.beta  : (usIsWhite ? saturatingAdd32Core(bounds.alpha, 1) : bounds.beta);

        int32_t score = 0;
	//FIXME: Trasformare in funzione helper if-else troppo grande
        if (canReduce) {
            // LOGARITHMIC LMR. Higher divisor = less reduction = more conservative
            constexpr double LMR_C = 3.07;
            int32_t reduction = static_cast<int32_t>(std::log(static_cast<double>(ctx.depth)) 
                                                   * std::log(static_cast<double>(moveIndex)) 
                                                   / LMR_C);
            // Cap reduction: never reduce more than depth-3 to ensure at least 3 plies of real search remain
            reduction = std::clamp(reduction, static_cast<int32_t>(1), ctx.depth - 3);
            
            if (inConservativeEndgameLMR) {
                reduction = std::min<int32_t>(reduction, 1);
            }

            const int32_t reducedDepth = std::max(static_cast<int32_t>(1), childDepth - reduction);
            score = this->searchPosition(b, reducedDepth, searchAlpha, searchBeta, ctx.ply + 1,
                                         useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
            
            // ================================================================
            // PROPER 3-STEP LMR RE-SEARCH
            // ================================================================
            // Step 1: reduced depth + null window 
            // Step 2: if fail -> full depth + null window  
            // Step 3: if still fail -> full depth + full window (PVS re-search)
            const bool reducedFailed = shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);

            if (reducedFailed && reducedDepth < childDepth) {
                // Step 2: full depth, null window — cheap verification
                score = this->searchPosition(b, childDepth, searchAlpha, searchBeta, ctx.ply + 1,
                                             useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
            }

            // Step 3: full depth, full window — only if null-window still fails
            const bool shouldResearch = !isFirstMove && shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
            
            if (shouldResearch) {
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1,
                                             useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
            }
        } else {
            score = this->searchPosition(b, childDepth, searchAlpha, searchBeta, ctx.ply + 1,
                                         useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);

            if (!isFirstMove) {
                const bool shouldResearch = shouldResearchPVS(score, searchAlpha, searchBeta, usIsWhite);
                if (shouldResearch) {
                    score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1,
                                                 useTT, allowTTWrite, allowHeuristicUpdates, &m, ctx.nodeCounter);
                }
            }
        }

        b.undoMove(m, state);
        searchedAnyMove = true;

        if (this->searchInterrupted.load(std::memory_order_relaxed)) {
            break;
        }

        // Track quiet moves for history malus (before checking cutoff)
        if (isQuietMove && numSearchedQuiets < MAX_QUIETS_TRACKED) {
	    //FIXME: Rendiamo esplicita l'incremento di numSearchedQuiets
            searchedQuiets[numSearchedQuiets++] = {m.from.index, m.to.index};
        }

        this->updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

        // Beta cutoff: check if the score causes a cutoff, then update killer/history
        // bounds.alpha >= bounds.beta means window collapsed (different condition!)
	//FIXME: Scrivere se vara prima codizione e falsa seconda break
	//FIXME: Scrivere se vere prima codizione E seconda codizione allora corpo della funzione
        if (isBetaCutoff(best, bounds.alpha, bounds.beta, usIsWhite)) {
            if (allowHeuristicUpdates) {
                this->updateKillerAndHistoryOnBetaCutoff(b, m, ctx.depth, ctx.ply, ctx.activeColor,
                                                      history, killerMoves, ctx.previousMove);

                // HISTORY MALUS: Penalize all quiet moves searched before the cutoff move
                // These moves were tried but failed to produce a cutoff, so they deserve lower history score
                if (isQuietMove) { // Only if the cutoff move itself is quiet
                    const int colorIndex = (ctx.activeColor == chess::Board::WHITE) ? 0 : 1;
                    const int malus = -static_cast<int>((ctx.depth + 1) * (ctx.depth + 1));
                    // GRAVITY FORMULA (symmetric with bonus side in engine.cpp):
                    // h += malus - h * |malus| / MAX_HISTORY
                    // This naturally decays toward bounds instead of hard-clamping,
                    // which caused asymmetric ordering: bad sacrifices weren't
                    // penalised fast enough relative to how quickly good moves
                    // were promoted.
                    constexpr int32_t MAX_HISTORY = 16384;
                    for (int i = 0; i < numSearchedQuiets - 1; ++i) {
                        int16_t& h = history[colorIndex][searchedQuiets[i].from][searchedQuiets[i].to];
                        int32_t hScore = static_cast<int32_t>(h);
                        hScore += malus - hScore * std::abs(malus) / MAX_HISTORY;
                        h = clampHeuristic16Core(hScore);
                    }
                }
            }
            break;
        }
        ++moveIndex;
    }

    if (!searchedAnyMove && this->searchInterrupted.load(std::memory_order_relaxed)) {
        return SearchMoveResult{bestMove, ctx.staticEval};
    }

    return SearchMoveResult{bestMove, best};
}

int32_t Engine::searchPosition(chess::Board& b, int32_t depth, int32_t alpha, int32_t beta, int ply,
                               bool useTT, bool allowTTWrite, bool allowHeuristicUpdates,
                               const chess::Board::Move* previousMove, uint64_t* nodeCounter, bool allowNullMove) noexcept {
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

    //FIXME: Rendere unica dichiarazione
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &this->nodesSearched;
    ++(*counter);

    if (this->shouldAbortSearch()) {
        this->searchInterrupted.store(true, std::memory_order_relaxed);
        return this->evaluate(b);
    }

    // avoid stack overflow and out-of-bounds access on killerMoves/history
    if (ply >= MAX_PLY - 1) {
        return this->evaluate(b);
    }

    if (depth <= 0) {
        return this->quiescenceSearch(b, alpha, beta, ply, useTT, counter);
    }

    // PV node detection (full window vs null window), deterministic by construction.
    const bool isPVNode = (static_cast<int64_t>(beta) - static_cast<int64_t>(alpha) > 1);

    // =========================================================================
    // MATE DISTANCE PRUNING
    // =========================================================================
    // If we already found a mate shorter than what this node could possibly produce,
    // prune immediately. This significantly speeds up mate searches.
    if (ply > 0) {
        // Best possible score for side to move: mate in (ply+1) moves
        // Worst possible score: getting mated in ply moves
        const int32_t matingAlpha = NEG_INF + ply;
        const int32_t matingBeta  = POS_INF - ply;
        if (alpha < matingAlpha) alpha = matingAlpha;
        if (beta > matingBeta)   beta = matingBeta;
        if (alpha >= beta) return alpha;
    }

    //FIXME: Mettere questa codizione dentro if di prima
    if (ply > 0 && b.isTwofoldRepetition()) {
        return repetitionDrawScore(b);
    }
    
    //FIXME: Mettere come precondizione?
    if (b.isFiftyMoveRule()) [[unlikely]] return 0;

    const uint64_t heavyMaterial =
        b.pawns_bb[0] | b.pawns_bb[1] |
        b.rooks_bb[0] | b.rooks_bb[1] |
        b.queens_bb[0] | b.queens_bb[1];
    if (heavyMaterial == 0ULL && hasInsufficientMaterialDraw(b)) [[unlikely]] {
        return 0;
    }

    const uint64_t hashKey = b.getHash();
    AlphaBeta bounds{alpha, beta}; // Prepare search structures
    int32_t score = 0;

    // Handle terminal nodes, check extensions, and transposition table lookups
    if (useTT && this->handleSearchPrelude(depth, bounds, score, hashKey)) {
        return score;
    }

    SearchNodeState node{};
    node.activeColor = b.getActiveColor();
    node.usIsWhite = (node.activeColor == chess::Board::WHITE);
    node.inCheck = b.inCheck(node.activeColor);
    node.isPVNode = isPVNode;
    const int nonPawnMajorsAll = __builtin_popcountll(
        b.knights_bb[0] | b.knights_bb[1] |
        b.bishops_bb[0] | b.bishops_bb[1] |
        b.rooks_bb[0]   | b.rooks_bb[1]   |
        b.queens_bb[0]  | b.queens_bb[1]);
    node.isPawnEndgameForPruning =
        ((b.pawns_bb[0] | b.pawns_bb[1]) != 0ULL) && (nonPawnMajorsAll <= 4);

    node.staticEval = (ply > 0 && !node.inCheck) ? this->evaluate(b) : 0;

    const int nonPawnMajors = __builtin_popcountll(
        b.knights_bb[node.usIsWhite ? 0 : 1] | b.bishops_bb[node.usIsWhite ? 0 : 1] |
        b.rooks_bb[node.usIsWhite ? 0 : 1]   | b.queens_bb[node.usIsWhite ? 0 : 1]);
    const int32_t nmpEvalGate = node.usIsWhite
        ? (node.staticEval + 100)
        : (node.staticEval - 100);
    const bool canNullMove = allowNullMove
        && !node.isPVNode
        && !node.inCheck
        && ply > 0
        && depth >= 6
        && nonPawnMajors >= 3
        && isBetaCutoff(nmpEvalGate, alpha, beta, node.usIsWhite);
    if (canNullMove
        && this->tryNullMovePruning(b, node, depth, alpha, beta, ply,
                                    useTT, allowTTWrite, allowHeuristicUpdates,
                                    counter, score)) {
        return score;
    }
    //FIXME: Eliminare numero magico
    const bool canReverseFutilityPrune =
        !node.isPVNode && !node.inCheck && !node.isPawnEndgameForPruning && ply > 0 && depth <= 3;
    if (canReverseFutilityPrune
        && this->tryReverseFutilityPruning(b, node, depth, alpha, beta, ply, score)) {
        return score;
    }

    SearchContext ctx{depth, bounds.alpha, bounds.beta, ply, node.activeColor,
                      previousMove, node.staticEval, node.inCheck, node.isPVNode, counter};
    MoveList<chess::Board::Move> moves = MoveGenerator::generateLegalMoves(b);
    if (moves.is_empty()) {
        return node.inCheck
            ? (node.usIsWhite ? (NEG_INF + ply) : (POS_INF - ply))
            : stalemateScoreFromMaterialDelta(getMaterialDelta(b));
    }

    const bool hasHashMove = this->sortLegalMoves(
        moves, ply, b, node.usIsWhite, hashKey, ctx.previousMove);

    if (!hasHashMove && depth >= 6 && ply > 0) {
        ctx.depth -= 1;
    }

    const int32_t alphaOrig = bounds.alpha;
    const int32_t betaOrig = bounds.beta;

    SearchMoveResult result = this->searchMoves(b, moves, node.usIsWhite, ctx, bounds, useTT, allowHeuristicUpdates, allowTTWrite);
    int32_t best = result.score;

    if (this->searchInterrupted.load(std::memory_order_relaxed)) {
        return this->evaluate(b);
    }

    if (useTT && allowTTWrite) {
        const auto flag = tt::determineFlag(best, alphaOrig, betaOrig);
        
        const uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(
            result.move.from.index, result.move.to.index, result.move.promotionPiece);

        this->tt.store(hashKey, static_cast<uint8_t>(ctx.depth), clampToTTScore(best),
                       static_cast<uint8_t>(flag), encodedMove);
    }
    return best;
}

} // namespace engine
