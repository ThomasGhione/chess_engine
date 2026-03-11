#include "searcher.hpp"
#include "../movelist.hpp"
#include "../eval/evaluator.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>

namespace engine {

// ============================================================================
// Helper Functions - Private static members of Searcher
// ============================================================================

int16_t Searcher::clampHeuristic16(int32_t value) noexcept {
    static constexpr int32_t MIN_I16 = -32768;
    static constexpr int32_t MAX_I16 = 32767;
    return static_cast<int16_t>(std::clamp(value, MIN_I16, MAX_I16));
}

int32_t Searcher::saturatingAdd32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t sum = static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs);
    if (sum > static_cast<int64_t>(POS_INF)) 
        return POS_INF;
    if (sum < static_cast<int64_t>(NEG_INF)) 
        return NEG_INF;
    return static_cast<int32_t>(sum);
}

int32_t Searcher::saturatingSub32(int32_t lhs, int32_t rhs) noexcept {
    const int64_t diff = static_cast<int64_t>(lhs) - static_cast<int64_t>(rhs);
    if (diff > static_cast<int64_t>(POS_INF)) 
        return POS_INF;
    if (diff < static_cast<int64_t>(NEG_INF)) 
        return NEG_INF;
    return static_cast<int32_t>(diff);
}

int32_t Searcher::stalemateScoreFromMaterialDelta(int32_t matDelta) noexcept {
    static constexpr int32_t STALEMATE_MATERIAL_THRESHOLD = 200;
    static constexpr int32_t STALEMATE_DRAW_PENALTY_MINOR = 50;
    static constexpr int32_t STALEMATE_DRAW_PENALTY_MAJOR = 150;
    
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) return 0;
    
    const int32_t advantage = std::abs(matDelta);
    const int32_t scaledPenalty = 
        STALEMATE_DRAW_PENALTY_MINOR + (advantage - STALEMATE_MATERIAL_THRESHOLD) / 2;
    const int32_t stalematePenalty = std::clamp<int32_t>(
        scaledPenalty, STALEMATE_DRAW_PENALTY_MINOR, STALEMATE_DRAW_PENALTY_MAJOR);
    return (matDelta > 0) ? -stalematePenalty : stalematePenalty;
}

int32_t Searcher::repetitionDrawScore(const chess::Board& b) noexcept {
    constexpr int32_t STALEMATE_MATERIAL_THRESHOLD = 200;
    const int32_t matDelta = Evaluator::getMaterialDelta(b);
    
    if (std::abs(matDelta) <= STALEMATE_MATERIAL_THRESHOLD) {
        return 0;
    }
    
    const int32_t contempt = std::min(static_cast<int32_t>(200), std::abs(matDelta) / 2);
    return (matDelta > 0) ? -contempt : contempt;
}

bool Searcher::hasInsufficientMaterialDraw(const chess::Board& b) noexcept {
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

void Searcher::doMoveWithPromotion(chess::Board& b, const chess::Board::Move& m, 
                                   chess::Board::MoveState& state) noexcept {
    char promo = m.promotionPiece;
    if (promo == '\0') promo = 'q';
    b.doMove(m, state, promo);
}

bool Searcher::isKillerMove(const chess::Board::Move& m, int ply) const noexcept {
    if (ply < 0 || ply >= SearchState::MAX_PLY) return false;
    const auto& km1 = state_.killerMoves[0][ply];
    const auto& km2 = state_.killerMoves[1][ply];
    return (m.from.index == km1.from.index && m.to.index == km1.to.index)
        || (m.from.index == km2.from.index && m.to.index == km2.to.index);
}

void Searcher::updateMinMax(bool usIsWhite, int32_t score, int32_t& alpha, int32_t& beta,
                            int32_t& bestScore, chess::Board::Move& bestMove, 
                            const chess::Board::Move& currentMove) noexcept {
    const bool isBetter = usIsWhite ? (score > bestScore) : (score < bestScore);
    if (isBetter) {
        bestScore = score;
        bestMove = currentMove;
    }
    if (usIsWhite) {
        if (score > alpha) alpha = score;
    } else {
        if (score < beta) beta = score;
    }
}

// ============================================================================
// SearchState implementation
// ============================================================================

void SearchState::reset() noexcept {
    std::memset(history, 0, sizeof(history));
    std::memset(killerMoves, 0, sizeof(killerMoves));
    std::memset(counterMoves, 0, sizeof(counterMoves));
    std::memset(captureHistory, 0, sizeof(captureHistory));
    nodesSearched = 0;
    interrupted.store(false, std::memory_order_relaxed);
}

void SearchState::softResetHistory() noexcept {
    // Decay history scores instead of full reset
    for (int c = 0; c < 2; ++c) {
        for (int from = 0; from < 64; ++from) {
            for (int to = 0; to < 64; ++to) {
                history[c][from][to] /= 2;
            }
        }
    }
    
    // Reset killers and counters
    std::memset(killerMoves, 0, sizeof(killerMoves));
    std::memset(counterMoves, 0, sizeof(counterMoves));
}

// ============================================================================
// SearchConfig presets
// ============================================================================

SearchConfig SearchConfig::standard() noexcept {
    SearchConfig cfg;
    cfg.maxDepth = 10;
    cfg.maxThreads = 1;
    cfg.useTT = true;
    cfg.useLMR = true;
    cfg.useNullMove = true;
    cfg.useFutilityPruning = true;
    cfg.useLateMoveReductions = true;
    cfg.useAspirationWindows = true;
    cfg.nullMoveReductionBase = 3;
    cfg.nullMoveReductionDepthDiv = 8;
    cfg.futilityMarginMG = 260;
    cfg.futilityMarginEG = 170;
    return cfg;
}

SearchConfig SearchConfig::conservative() noexcept {
    SearchConfig cfg = standard();
    cfg.useNullMove = false;
    cfg.useFutilityPruning = false;
    cfg.useLateMoveReductions = false;
    cfg.useAspirationWindows = false;
    return cfg;
}

SearchConfig SearchConfig::aggressive() noexcept {
    SearchConfig cfg = standard();
    cfg.futilityMarginEG = 60;
    cfg.nullMoveReductionBase = 4;
    return cfg;
}

// ============================================================================
// Searcher constructor and basic methods
// ============================================================================

Searcher::Searcher(SearchState& state, const SearchConfig& config, tt::TranspositionTable& tt) noexcept
    : state_(state)
    , config_(config)
    , tt_(tt) {
}

SearchResult Searcher::findBestMove(const chess::Board& position) noexcept {
    chess::Board workingBoard = position;
    state_.nodesSearched = 0;
    state_.interrupted.store(false, std::memory_order_relaxed);
    
    return runIterativeDeepening(workingBoard, 1, config_.maxDepth);
}

void Searcher::stop() noexcept {
    state_.interrupted.store(true, std::memory_order_release);
}

bool Searcher::shouldAbort() const noexcept {
    return state_.interrupted.load(std::memory_order_acquire);
}

} // namespace engine
