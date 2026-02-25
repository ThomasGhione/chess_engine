#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

static inline bool isForcingEvasion(const chess::Board& b,
                                    const chess::Board::Move& m,
                                    const chess::Coords& enPassant,
                                    bool hasEnPassant) noexcept {
    const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
    if (toPieceType != chess::Board::EMPTY) return true;

    const uint8_t fromPiece = b.get(m.from);
    const uint8_t fromType = fromPiece & chess::Board::MASK_PIECE_TYPE;
    if (fromType != chess::Board::PAWN) return false;

    const bool isPromotion = (m.to.rank() == chess::Board::promotionRank((fromPiece & chess::Board::MASK_COLOR) == chess::Board::WHITE));
    if (isPromotion) return true;

    return hasEnPassant
        && (m.to == enPassant)
        && (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index));
}

// ============================================================================
// QUIESCENCE SEARCH - Eliminates horizon effect
// ============================================================================
// Searches only tactical moves (captures, promotions) to find a quiet position
// This prevents the engine from stopping the search at a position where a capture sequence is ongoing
// Example: if we search to depth 0 during "Queen takes Pawn, Rook takes Queen", we'd evaluate
// as if we won a pawn, when in reality we're about to lose the Queen.
//
// Delta pruning: Skip captures that can't possibly raise alpha (even if the capture succeeds)
// SEE pruning: Skip losing captures (SEE < threshold)
//
// NOTE: We do NOT generate checks (non-capture) as they cause tree explosion
// Most modern engines only search captures + promotions in qsearch
int64_t Engine::quiescenceSearch(chess::Board& b, int64_t alpha, int64_t beta, int ply, bool useTT, uint64_t* nodeCounter) noexcept {
    uint64_t* counter = (nodeCounter != nullptr) ? nodeCounter : &this->nodesSearched;
    ++(*counter);

    // SAFETY: prevent stack overflow
    if (ply >= MAX_PLY - 1) {
        return this->evaluate(b);
    }

    // =========================================================================
    // TT PROBE IN QUIESCENCE SEARCH
    // =========================================================================
    // Probe the TT before doing any work. If we have a stored result for this
    // position at sufficient depth, we can return immediately. This avoids
    // re-evaluating identical tactical positions that arise via transpositions.
    if (useTT) {
        const uint64_t hashKey = b.getHash();
        {
            int64_t ttScore = 0;
            // Probe at depth 0 (qsearch is depth <= 0)
            if (this->tt.probe(hashKey, 0, alpha, beta, ttScore)) {
                return ttScore;
            }
        }
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
            MoveList<chess::Board::Move> evasions = Engine::generateLegalMoves(b);
            if (evasions.is_empty()) {
                return usIsWhite ? (NEG_INF + ply) : (POS_INF - ply);
            }
        }
        return this->evaluate(b);
    }

    // In-check nodes cannot use stand-pat or delta pruning.
    // We must search all legal evasions.
    if (inCheck) {
        MoveList<chess::Board::Move> evasions = Engine::generateLegalMoves(b);
        if (evasions.is_empty()) {
            return usIsWhite ? (NEG_INF + ply) : (POS_INF - ply);
        }

        MoveList<chess::Board::Move> forcingEvasions;
        MoveList<chess::Board::Move> quietEvasions;
        for (const auto& m : evasions) {
            if (isForcingEvasion(b, m, enPassant, hasEnPassant)) {
                forcingEvasions.emplace_back(m);
            } else {
                quietEvasions.emplace_back(m);
            }
        }

        int64_t best = Engine::initialBest(usIsWhite);
        auto searchEvasionSet = [&](const MoveList<chess::Board::Move>& evasionSet) -> bool {
            for (const auto& m : evasionSet) {
                chess::Board::MoveState state;
                doMoveWithPromotion(b, m, state);
                const int64_t score = this->quiescenceSearch(b, alpha, beta, ply + 1, useTT, counter);
                b.undoMove(m, state);

                if (Engine::isBetter(score, best, usIsWhite)) {
                    best = score;
                }

                updateBound(score, alpha, beta, usIsWhite);
                if (isBetaCutoff(score, alpha, beta, usIsWhite)) {
                    return true;
                }
            }
            return false;
        };

        // Two-pass evasion ordering:
        // 1) forcing evasions (captures/promotions), 2) quiet evasions.
        // This improves alpha-beta cutoffs in tactical check sequences.
        if (searchEvasionSet(forcingEvasions)) return cutoffValue(alpha, beta, usIsWhite);
        if (searchEvasionSet(quietEvasions)) return cutoffValue(alpha, beta, usIsWhite);

        return best;
    }

    // Stand-pat: current static evaluation
    // evaluate() returns score from white's perspective (positive = white winning)
    const int64_t standPat = this->evaluate(b);

    // Beta cutoff: position is too good for the active player
    if (isBetaCutoff(standPat, alpha, beta, usIsWhite)) {
        // Early cutoff - don't store in TT (too shallow, overhead not worth it)
        return cutoffValue(alpha, beta, usIsWhite);
    }

    // Update alpha/beta with stand-pat score
    updateBound(standPat, alpha, beta, usIsWhite);

    // ============================================================================
    // EARLY DELTA PRUNING - BEFORE move generation
    // ============================================================================
    // If stand-pat is so bad that even the best possible capture (Queen = 900cp)
    // plus a huge margin can't reach alpha/beta, skip move generation entirely.
    // This saves significant time by avoiding generateTacticalMoves() in hopeless positions.
    // In-check nodes are handled above and never reach this section.
    static constexpr int64_t EARLY_DELTA_MARGIN = 950; // Just Queen + tiny margin (more pruning)

    if (shouldDeltaPrune(standPat, EARLY_DELTA_MARGIN, alpha, beta, usIsWhite)) {
        // Early pruning - don't store in TT (too frequent, overhead not worth it)
        return usIsWhite ? alpha : beta; // Early delta cutoff (fail-low bound)
    }

    // ============================================================================
    // DYNAMIC DELTA PRUNING - Advanced version
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
    int64_t deltaMargin = QUEEN_VALUE; // Base: best single capture
    
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
    const int64_t materialBalance = usIsWhite ? standPat : -standPat;
    if (materialBalance < -400) {
        // Losing by 4+ pawns: add 150cp to delta (desperate but realistic)
        deltaMargin += 150;
    } else if (materialBalance < -200) {
        // Losing by 2 pawns: add 75cp (modest comeback attempt)
        deltaMargin += 75;
    }
    
    // Factor 3: Depth penalty - deeper in qsearch = more conservative
    const int qsearchDepth = ply - this->depth; // Approximate qsearch depth
    if (qsearchDepth > 5) {
        deltaMargin -= 50 * ((qsearchDepth - 5) / 5);
        deltaMargin = std::max(deltaMargin, static_cast<int64_t>(QUEEN_VALUE)); // Floor at Queen value
    }
    
    // Apply delta pruning with dynamic margin
    if (shouldDeltaPrune(standPat, deltaMargin, alpha, beta, usIsWhite)) {
        return cutoffValue(alpha, beta, usIsWhite);
    }

    // Generate only captures/promotions in qsearch (no non-capture checks).
    // In-check nodes are already handled above with full legal evasions.
    MoveList<chess::Board::Move> tacticalMoves = Engine::generateTacticalMoves(b, false, true, false, false);
    
    // No tactical moves: return stand-pat (quiet position reached)
    if (tacticalMoves.is_empty()) {
        return standPat;
    }

    // Sort tactical moves by MVV-LVA and SEE
    MoveList<ScoredMove> orderedMoves;
    
    // Dynamic SEE pruning threshold based on depth and material balance
    // The engine should NOT speculate with losing captures hoping for positional comp
    // Shallow qsearch (ply < 10): SEE >= -15cp (only tiny tactical losses)
    // Mid qsearch (10-20): SEE >= -8cp (very conservative)
    // Deep qsearch (ply >= 20): SEE >= 0cp (neutral or better only)
    const int64_t seeThreshold = (ply < 10) ? -15 : ((ply < 20) ? -8 : 0);
    
    for (const auto& m : tacticalMoves) {
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
        
        int64_t score = 0;
        
        if (isCapture) {
            // TODO test this better!!
            // ============================================================================
            // FUTILITY PRUNING IN QSEARCH
            // ============================================================================
            // Skip captures that can't possibly raise alpha, even if they win material.
            // This is aggressive pruning based on material value alone.
            const int64_t capturedValue = PIECE_VALUES[victimType];
            static constexpr int64_t FUTILITY_MARGIN = 100; // Minimal margin - prioritize material!
            
            // Check if this capture can possibly improve our position enough
            if (shouldDeltaPrune(standPat, capturedValue + FUTILITY_MARGIN, alpha, beta, usIsWhite)) {
                continue;
            }
            
            const int64_t see = staticExchangeEvaluation(b, m);

            // SEE-based pruning with dynamic threshold
            // Dynamic threshold is already strict enough (-16cp shallow, 0cp deep)
            // No need for additional hard cutoff (was redundant and too permissive at -300cp)
            if (see < seeThreshold) {
                continue;
            }
            
            // PER-MOVE DELTA PRUNING: prune captures that can't improve position
            // Even if this capture is "good" by SEE, if standPat + captureValue + margin
            // still can't reach alpha/beta, skip it
            static constexpr int64_t MOVE_DELTA_MARGIN = 100; // Minimal margin - material > position
            
            if (shouldDeltaPrune(standPat, see + MOVE_DELTA_MARGIN, alpha, beta, usIsWhite)) {
                continue; // Per-move delta pruning
            }
            
            // Score by MVV + SEE for better ordering
            // SEE-based ordering: captures with better SEE explored first
            score = 10000 + see; // Base + SEE value (can be negative for losing captures)
            score += MVV_TABLE[victimType];
            // Total: 10000 + see + MVV (1000-9000)
        } else {
            // Non-capture: must be a promotion
            if (isPromotion) {
                score = 9000; // Promotion (high priority)
            } else {
                continue;
            }
        }
        
        orderedMoves.emplace_back(m, score);
    }
    
    // If all captures were pruned, return stand-pat
    if (orderedMoves.is_empty()) {
        return standPat;
    }
    
    orderedMoves.sort();

    // Search tactical moves using MINIMAX (not negamax)
    int64_t best = standPat; // Initialize with stand-pat
    
    for (const auto& scoredMove : orderedMoves) {
        const auto& m = scoredMove.move;
        chess::Board::MoveState state;
        
        doMoveWithPromotion(b, m, state);
        
        // MINIMAX: recursively search with same alpha-beta window
        // The side switches automatically because b.doMove() changes activeColor
        const int64_t score = this->quiescenceSearch(b, alpha, beta, ply + 1, useTT, counter);
        
        b.undoMove(m, state);
        
        // Update best score
        if (isBetter(score, best, usIsWhite)) {
            best = score;
        }
        
        // Update alpha bound before checking beta cutoff
        updateBound(score, alpha, beta, usIsWhite);
        
        // Alpha-beta pruning
        if (isBetaCutoff(score, alpha, beta, usIsWhite)) {
            // Beta cutoff - don't store in TT (happens too frequently in qsearch)
            return cutoffValue(alpha, beta, usIsWhite);
        }
    }
    
    return best;
}

} // namespace engine
