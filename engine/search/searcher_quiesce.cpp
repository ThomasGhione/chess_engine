#include "searcher.hpp"
#include "../eval/evaluator.hpp"
#include "../bonus-malus/mvv_tables.hpp"
#include "../movegen/movegen.hpp"

namespace engine {

int32_t Searcher::quiesce(chess::Board& b, int32_t alpha, int32_t beta, int ply, uint64_t* nodeCounter) noexcept {
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &state_.nodesSearched;
    ++(*counter);

    if (shouldAbort()) {
        state_.interrupted.store(true, std::memory_order_relaxed);
        return Evaluator::evaluate(b);
    }

    // SAFETY: prevent stack overflow
    if (ply >= Engine::MAX_PLY - 1) {
        return Evaluator::evaluate(b);
    }

    // =========================================================================
    // TT PROBE IN QUIESCENCE SEARCH
    // =========================================================================
    // Probe the TT before doing any work. If we have a stored result for this
    // position at sufficient depth, we can return immediately.
    if (config_.useTT) {
        const uint64_t hashKey = b.getHash();
        int32_t ttScore = 0;
        int32_t ttAlpha = 0;
        int32_t ttBeta = 0;
        Engine::toTTProbeBounds(alpha, beta, ttAlpha, ttBeta);
        // Probe at depth 0 (qsearch is depth <= 0)
        if (tt_.probe(hashKey, 0, ttAlpha, ttBeta, ttScore))
            return static_cast<int32_t>(ttScore);
    }

    const uint8_t activeColor = b.getActiveColor();
    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    const uint8_t promotionRank = chess::Board::promotionRank(usIsWhite);
    const chess::Coords enPassant = b.getEnPassant();
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const bool inCheck = b.inCheck(activeColor);

    // ============================================================================
    // DEPTH LIMIT IN QUIESCENCE - Prevent explosion in complex tactical positions
    // ============================================================================
    static constexpr uint8_t MAX_QSEARCH_DEPTH = 32;
    if (ply >= MAX_QSEARCH_DEPTH) {
        // Do not return a stand-pat score from an in-check node without checking
        // whether this is actually checkmate.
        if (inCheck) {
            MoveList<chess::Board::Move> evasions = MoveGenerator::generateLegalMoves(b);
            if (evasions.is_empty()) {
                return usIsWhite ? (NEG_INF + ply) : (POS_INF - ply);
            }
        }
        return Evaluator::evaluate(b);
    }

    // In-check nodes cannot use stand-pat or delta pruning.
    // We must search all legal evasions.
    if (inCheck) {
        MoveList<chess::Board::Move> evasions = MoveGenerator::generateLegalMoves(b);
        if (evasions.is_empty()) {
            return usIsWhite ? (NEG_INF + ply) : (POS_INF - ply);
        }

        int32_t best = Engine::initialBest(usIsWhite);

        // Two-pass evasion ordering:
        // 1) forcing evasions (captures/promotions)
        // 2) quiet evasions.
        // This improves alpha-beta cutoffs in tactical check sequences.
        for (int pass = 0; pass < 2; ++pass) {
            const bool searchForcing = (pass == 0);
            for (const auto& m : evasions) {
                if (shouldAbort()) {
                    state_.interrupted.store(true, std::memory_order_relaxed);
                    return Evaluator::evaluate(b);
                }

                const bool isForcing = Engine::isForcingEvasion(b, m, enPassant, hasEnPassant);
                if (isForcing != searchForcing) {
                    continue;
                }

                chess::Board::MoveState state;
                doMoveWithPromotion(b, m, state);
                const int32_t score = quiesce(b, alpha, beta, ply + 1, counter);
                b.undoMove(m, state);

                if (Engine::isBetter(score, best, usIsWhite)) {
                    best = score;
                }

                Engine::updateBound(score, alpha, beta, usIsWhite);
                if (Engine::isBetaCutoff(score, alpha, beta, usIsWhite)) {
                    return Engine::cutoffValue(alpha, beta, usIsWhite);
                }
            }
        }

        return best;
    }

    // Stand-pat: current static evaluation
    // evaluate() returns score from white's perspective (positive = white winning)
    const int32_t standPat = Evaluator::evaluate(b);

    // Beta cutoff: position is too good for the active player
    if (Engine::isBetaCutoff(standPat, alpha, beta, usIsWhite)) {
        // Early cutoff - don't store in TT (too shallow, overhead not worth it)
        return Engine::cutoffValue(alpha, beta, usIsWhite);
    }

    // Update alpha/beta with stand-pat score
    Engine::updateBound(standPat, alpha, beta, usIsWhite);

    // ============================================================================
    // EARLY DELTA PRUNING - BEFORE move generation
    // ============================================================================
    // If stand-pat is so bad that even the best possible capture (Queen = 900cp)
    // plus a huge margin can't reach alpha/beta, skip move generation entirely.
    // This saves significant time by avoiding generateTacticalMoves() in hopeless positions.
    // In-check nodes are handled above and never reach this section.
    static constexpr int32_t EARLY_DELTA_MARGIN = 950; // Just Queen + tiny margin (more pruning)

    if (Engine::shouldDeltaPrune(standPat, EARLY_DELTA_MARGIN, alpha, beta, usIsWhite)) {
        return usIsWhite ? alpha : beta; // Early delta cutoff (fail-low bound)
    }

    // ============================================================================
    // DYNAMIC DELTA PRUNING 
    // ============================================================================
    // Delta pruning: if even the best possible capture can't improve our position enough
    // to affect the search result, we can prune the entire qsearch subtree.
    //
    // Dynamic factors:
    // 1. Base margin: Queen value (biggest possible single capture)
    // 2. Promotion bonus: if we have pawns close to promotion
    // 3. Material deficit bonus: if we're losing, we need comebacks (larger delta)
    // 4. Depth penalty: deeper in qsearch = more conservative (reduce delta)
    
    // Compute dynamic delta margin
    int32_t deltaMargin = QUEEN_VALUE; // Base: best single capture
    
    // Factor 1: Check for near-promotion pawns (7th/2nd rank)
    const int side = chess::Board::colorToIndex(activeColor);
    const uint64_t ourPawns = b.pawns_bb[side];
    const uint64_t nearPromoPawns = usIsWhite 
        ? (ourPawns & 0x00FF000000000000ULL) // Rank 7 for white
        : (ourPawns & 0x000000000000FF00ULL); // Rank 2 for black
    
    if (nearPromoPawns) {
        deltaMargin += 150; // Conservative promotion bonus
    }
    
    // Factor 2: Material deficit - if we're losing, allow more speculative lines
    const int32_t materialBalance = usIsWhite
        ? standPat
        : (standPat == std::numeric_limits<int32_t>::min() ? std::numeric_limits<int32_t>::max() : -standPat);
    if (materialBalance < -400) {
        // Losing by 4+ pawns: add 150cp to delta (desperate but realistic)
        deltaMargin += 150;
    } else if (materialBalance < -200) {
        // Losing by 2 pawns: add 75cp (modest comeback attempt)
        deltaMargin += 75;
    }
    
    // Factor 3: Depth penalty - deeper in qsearch = more conservative
    const int qsearchDepth = std::max(0, ply - static_cast<int>(config_.maxDepth)); // Approximate qsearch depth
    if (qsearchDepth > 5) {
        deltaMargin -= 50 * ((qsearchDepth - 5) / 5);
        deltaMargin = std::max(deltaMargin, static_cast<int32_t>(QUEEN_VALUE)); // Floor at Queen value
    }
    
    // Apply delta pruning with dynamic margin
    if (Engine::shouldDeltaPrune(standPat, deltaMargin, alpha, beta, usIsWhite)) {
        // Delta prune is a fail-low condition: return the bound we failed to reach.
        return usIsWhite ? alpha : beta;
    }

    // Generate only captures/promotions in qsearch (no non-capture checks).
    // In-check nodes are already handled above with full legal evasions.
    MoveList<chess::Board::Move> tacticalMoves = MoveGenerator::generateTacticalMoves(b, false, true, false, false);
    
    // No tactical moves: return stand-pat (quiet position reached)
    if (tacticalMoves.is_empty()) {
        return standPat;
    }

    // Sort tactical moves by MVV-LVA and SEE using compact parallel score storage.
    int32_t tacticalScores[MAX_MOVES] {};
    int filteredCount = 0;
    
    // Dynamic SEE pruning threshold based on depth and material balance
    // The engine should NOT speculate with losing captures hoping for positional comp
    // Shallow qsearch (ply < 10): SEE >= -15cp (only tiny tactical losses)
    // Mid qsearch (10-20): SEE >= -8cp (very conservative)
    // Deep qsearch (ply >= 20): SEE >= 0cp (neutral or better only)
    const int32_t seeThreshold = (ply < 10) ? -15 : ((ply < 20) ? -8 : 0);
    
    for (int i = 0; i < tacticalMoves.size; ++i) {
        const chess::Board::Move m = tacticalMoves[static_cast<size_t>(i)];
        const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
        const bool isPromotion = (fromPieceType == chess::Board::PAWN) && (m.to.rank() == promotionRank);
        const bool isEpCapture = hasEnPassant
            && fromPieceType == chess::Board::PAWN
            && toPieceType == chess::Board::EMPTY
            && (m.to == enPassant)
            && (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index));
        const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
        const uint8_t victimType = isEpCapture ? static_cast<uint8_t>(chess::Board::PAWN) : toPieceType;
        
        int32_t score = 0;
        
        if (isCapture) {
            // TODO test this better!!
            // ============================================================================
            // FUTILITY PRUNING IN QSEARCH
            // ============================================================================
            // Skip captures that can't possibly raise alpha, even if they win material.
            // This is aggressive pruning based on material value alone.
            const int32_t capturedValue = Engine::PIECE_VALUES[victimType];
            static constexpr int32_t FUTILITY_MARGIN = 100; // Minimal margin - prioritize material!
            
            // Check if this capture can possibly improve our position enough
            if (Engine::shouldDeltaPrune(standPat, capturedValue + FUTILITY_MARGIN, alpha, beta, usIsWhite)) {
                continue;
            }
            
            const int32_t see = staticExchangeEvaluation(b, m);

            // SEE-based pruning with dynamic threshold
            // Dynamic threshold is already strict enough (-16cp shallow, 0cp deep)
            // No need for additional hard cutoff (was redundant and too permissive at -300cp)
            if (see < seeThreshold) {
                continue;
            }
            
            // PER-MOVE DELTA PRUNING: prune captures that can't improve position
            // Even if this capture is "good" by SEE, if standPat + captureValue + margin
            // still can't reach alpha/beta, skip it
            static constexpr int32_t MOVE_DELTA_MARGIN = 100; // Minimal margin - material > position
            
            if (Engine::shouldDeltaPrune(standPat, see + MOVE_DELTA_MARGIN, alpha, beta, usIsWhite)) {
                continue; // Per-move delta pruning
            }
            
            // Score by MVV + SEE for better ordering
            // SEE-based ordering: captures with better SEE explored first
            score = Engine::clampQMoveScore(10000 + see + MVV_TABLE[victimType]);
            // Total: 10000 + see + MVV (1000-9000)
        } else {
            // Non-capture: must be a promotion
            if (isPromotion) {
                score = 9000; // Promotion (high priority)
            } else {
                continue;
            }
        }

        tacticalMoves[static_cast<size_t>(filteredCount)] = m;
        tacticalScores[filteredCount] = score;
        ++filteredCount;
    }

    tacticalMoves.size = filteredCount;

    // If all captures were pruned, return stand-pat
    if (tacticalMoves.is_empty()) {
        return standPat;
    }

    // Insertion-sort tactical moves + scores together (descending).
    for (int i = 1; i < tacticalMoves.size; ++i) {
        const chess::Board::Move keyMove = tacticalMoves[static_cast<size_t>(i)];
        const int32_t keyScore = tacticalScores[i];
        int j = i - 1;
        while (j >= 0 && tacticalScores[j] < keyScore) {
            tacticalScores[j + 1] = tacticalScores[j];
            tacticalMoves[static_cast<size_t>(j + 1)] = tacticalMoves[static_cast<size_t>(j)];
            --j;
        }
        tacticalScores[j + 1] = keyScore;
        tacticalMoves[static_cast<size_t>(j + 1)] = keyMove;
    }

    // Save original bounds for TT flag determination
    const int32_t alphaOrig = alpha;
    const int32_t betaOrig = beta;

    int32_t best = standPat;
    
    for (const auto& m : tacticalMoves) {
        if (shouldAbort()) {
            state_.interrupted.store(true, std::memory_order_relaxed);
            return Evaluator::evaluate(b);
        }
        chess::Board::MoveState state;
        
        doMoveWithPromotion(b, m, state);
        
        // MINIMAX: recursively search with same alpha-beta window
        // The side switches automatically because b.doMove() changes activeColor
        const int32_t score = quiesce(b, alpha, beta, ply + 1, counter);
        
        b.undoMove(m, state);
        
        if (Engine::isBetter(score, best, usIsWhite)) {
            best = score;
        }
        
        Engine::updateBound(score, alpha, beta, usIsWhite);
        
        if (Engine::isBetaCutoff(score, alpha, beta, usIsWhite)) {
            // TT STORE on beta cutoff (lower/upper bound)
            if (config_.useTT) {
                const uint64_t hashKey = b.getHash();
                const auto flag = tt::determineFlag(best, alphaOrig, betaOrig);
                tt_.store(hashKey, 0, Engine::clampToTTScore(Engine::cutoffValue(alpha, beta, usIsWhite)),
                           static_cast<uint8_t>(flag));
            }
            return Engine::cutoffValue(alpha, beta, usIsWhite);
        }
    }
    
    // =========================================================================
    // TT STORE IN QUIESCENCE - cache the result for transposition reuse.
    // Without this, tactical refutations discovered in qsearch are thrown away
    // and recomputed every time the same position is reached via a different
    // move order, wasting significant search effort.
    // =========================================================================
    if (config_.useTT) {
        const uint64_t hashKey = b.getHash();
        const auto flag = tt::determineFlag(best, alphaOrig, betaOrig);
        tt_.store(hashKey, 0, Engine::clampToTTScore(best), static_cast<uint8_t>(flag));
    }

    return best;
}

int32_t Searcher::staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept {
    const uint8_t toSq = m.to.index;
    const uint8_t fromSq = m.from.index;

    const int sideActive = b.getActiveColor() == chess::Board::WHITE ? 0 : 1;
    const int sidePassive = sideActive ^ 1;

    // Value of the initially captured piece
    uint8_t capturedType = b.get(toSq) & chess::Board::MASK_PIECE_TYPE;
    if (capturedType == chess::Board::EMPTY) {
        // En passant: captures a pawn
        capturedType = chess::Board::PAWN;
    }

    // Canonical SEE (swap algorithm):
    // gain[0] = value(victim)
    // for each recapture i:
    //   gain[i] = value(captured_piece) - gain[i-1]
    // where captured_piece is the piece that just moved to the target square in the previous ply.
    constexpr int MAX_SEE_DEPTH = 16;
    int32_t gain[MAX_SEE_DEPTH];
    gain[0] = Engine::PIECE_VALUES[capturedType];

    // Simulate the exchange on local occupancy
    uint64_t occ = b.getPiecesBitMap();
    occ ^= chess::Board::bitMask(fromSq); // remove the piece that makes the first capture from its square

    // After the initial move, the piece now "on target" is our initial attacker.
    uint8_t capturedOnTargetType = b.get(fromSq) & chess::Board::MASK_PIECE_TYPE;

    int depth = 1;
    int side = sidePassive; // the opponent captures next

    // Do not use piece-value early exits here.
    // They can misclassify captures like QxP as losing without checking
    // recaptures, pins, x-rays and overloaded defenders.

    while (depth < MAX_SEE_DEPTH) {
        // Find the least valuable attacker toward the target square
        uint8_t attacker = Engine::getLeastValuableAttackerTo(b, toSq, occ, side);
        if (attacker == 64) break;

        // Determine attacker type using the piece bitboards AND the simulated occupancy
        // (safer than querying b.get(...) which reflects the original board only).
        const uint64_t attackerMask = chess::Board::bitMask(attacker);
        uint8_t currentAttackerType = chess::Board::PAWN; // default/fallback
        if ((b.pawns_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::PAWN;
        else if ((b.knights_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::KNIGHT;
        else if ((b.bishops_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::BISHOP;
        else if ((b.rooks_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::ROOK;
        else if ((b.queens_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::QUEEN;
        else if ((b.kings_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::KING;

        // Record the gain for this side
        gain[depth] = Engine::PIECE_VALUES[capturedOnTargetType] - gain[depth - 1];

        // Update the piece that was captured
        capturedOnTargetType = currentAttackerType;

        // Remove this attacker from the occupancy
        occ ^= attackerMask;

        // Switch to opponent
        side = 1 - side;
        depth++;
    }

    // Minimax through the gains: start with the deepest gain and work backward
    for (int i = depth - 1; i > 0; --i) {
        if (gain[i] < gain[i - 1]) {
            gain[i - 1] = gain[i];
        }
    }

    return gain[0];
}

} // namespace engine
