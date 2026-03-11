#include "searcher.hpp"
#include "../eval/evaluator.hpp"

namespace engine {

bool Searcher::tryNullMovePruning(chess::Board& b, const SearchNodeState& node,
                                  int32_t depth, int32_t alpha, int32_t beta, int ply,
                                  uint64_t* nodeCounter, int32_t& outScore) noexcept {
    const int32_t reduction = config_.nullMoveReductionBase + depth / config_.nullMoveReductionDepthDiv;

    chess::Board::MoveState nullState;
    b.doNullMove(nullState);

    const int32_t nullScore = search(b, depth - reduction, alpha, beta, ply + 1,
                                     nullptr, nodeCounter, false);

    b.undoNullMove(nullState);

    if (!Engine::isBetaCutoff(nullScore, alpha, beta, node.usIsWhite)) {
        return false;
    }

    bool confirmedCutoff = true;
    if (depth >= 10) {
        const int32_t verifyScore = search(b, depth - reduction, alpha, beta, ply,
                                          nullptr, nodeCounter, false);
        confirmedCutoff = Engine::isBetaCutoff(verifyScore, alpha, beta, node.usIsWhite);
    }

    if (!confirmedCutoff) {
        return false;
    }

    if (!b.hasAnyLegalMove(node.activeColor)) {
        outScore = stalemateScoreFromMaterialDelta(Evaluator::getMaterialDelta(b));
        return true;
    }

    outScore = Engine::cutoffValue(alpha, beta, node.usIsWhite);
    return true;
}

bool Searcher::tryReverseFutilityPruning(chess::Board& b, const SearchNodeState& node,
                                         int32_t depth, int32_t alpha, int32_t beta, int ply,
                                         int32_t& outScore) noexcept {
    constexpr int32_t RFP_MARGIN_PER_DEPTH = 110;

    if (node.isPVNode || node.inCheck || node.isPawnEndgameForPruning || ply <= 0 || depth > 3) {
        return false;
    }

    const int32_t rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
    const int32_t rfpScore = node.usIsWhite
        ? (node.staticEval - rfpMargin)
        : (node.staticEval + rfpMargin);
    if (!Engine::isBetaCutoff(rfpScore, alpha, beta, node.usIsWhite)) {
        return false;
    }

    if (!b.hasAnyLegalMove(node.activeColor)) {
        outScore = stalemateScoreFromMaterialDelta(Evaluator::getMaterialDelta(b));
        return true;
    }

    outScore = node.staticEval;
    return true;
}

void Searcher::updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m,
                                                  int32_t depth, int ply, uint8_t us,
                                                  const chess::Board::Move* previousMove) noexcept {
    if (ply >= SearchState::MAX_PLY) return;
    
    const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
    const bool isEpCapture = isEnPassantCapture(b, m);
    const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
    
    if (!isCapture) {
        // Update killer moves and history ONLY for quiet moves
        // Killer moves: best quiet move that produced a cutoff at this ply
        // Pre-compute piece info once
        const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
        const int fromIndex = m.from.index;
        const int toIndex = m.to.index;
        
        // Update killer moves
        if (ply < SearchState::MAX_PLY && fromPieceType != chess::Board::PAWN) {
            // Shift old killers back
            state_.killerMoves[ply][1] = state_.killerMoves[ply][0];
            state_.killerMoves[ply][0] = m;
        }
        
        // Update counter-move
        if (previousMove && ply >= 1 && ply < SearchState::MAX_PLY) {
            // Encode move to uint16_t for counter-move storage
            uint16_t encodedMove = tt::TranspositionTable::Entry::encodeMove(m.from.index, m.to.index, m.promotionPiece);
            state_.counterMoves[previousMove->from.index][previousMove->to.index] = encodedMove;
        }
        
        // Update history heuristic
        // Bonus formula: h += bonus - h * |bonus| / MAX_HISTORY
        // This uses "gravity" toward bounds to prevent overflow and maintain
        // a bounded [-32768, 32767] range within the int16_t naturally.
        // See: https://www.chessprogramming.org/Gravity-Heuristics.html
        const int32_t bonus = depth * depth;
        constexpr int32_t MAX_HISTORY = 16384;
        auto& h = state_.history[colorIndex][fromIndex][toIndex];
        int32_t hScore = static_cast<int32_t>(h);
        hScore += bonus - hScore * std::abs(bonus) / MAX_HISTORY;
        h = clampHeuristic16(hScore);
    } else {
        // Update capture history for captures
        if (ply < SearchState::MAX_PLY && toPieceType != chess::Board::EMPTY) {
            const int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
            const int32_t bonus = depth * depth;
            constexpr int32_t MAX_CAP_HISTORY = 8192;
            auto& ch = state_.captureHistory[colorIndex][m.from.index][m.to.index][toPieceType];
            int32_t chScore = static_cast<int32_t>(ch);
            chScore += bonus - chScore * std::abs(bonus) / MAX_CAP_HISTORY;
            ch = clampHeuristic16(chScore);
        }
    }
}

} // namespace engine
