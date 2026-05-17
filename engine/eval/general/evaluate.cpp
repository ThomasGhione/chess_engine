#include "../evaluator.hpp"

namespace engine {

namespace {

struct EvalCacheEntry {
    uint64_t key = std::numeric_limits<uint64_t>::max();
    int32_t score = 0;
    uint8_t valid = 0;
};

static constexpr size_t EVAL_CACHE_SIZE = 1u << 11; // 2048 entries (~32 KiB), tests/perf tuned.
static constexpr uint64_t EVAL_CACHE_MASK = EVAL_CACHE_SIZE - 1u;
static constexpr uint64_t FULLMOVE_CACHE_SALT = 0x9E3779B97F4A7C15ULL;

} // namespace

int32_t Evaluator::evaluateCheckmate(const chess::Board& board) noexcept {
    // Negamax / side-to-move relative: on a checkmated board the side to move
    // is the one mated. -POS_INF (not NEG_INF=INT32_MIN, which cannot be
    // safely negated by the negamax recursion).
    (void)board;
    return -POS_INF;
}

int32_t Evaluator::evaluate(const chess::Board& board) noexcept {
    const uint8_t activeColor = board.getActiveColor();
    const bool whiteToMove = (activeColor == chess::Board::WHITE);
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0) [[unlikely]] {
        // Side-to-move relative: losing our king is -POS_INF, capturing the
        // opponent's is +POS_INF.
        if (board.kings_bb[0] == 0) return whiteToMove ? -POS_INF : POS_INF;
        return whiteToMove ? POS_INF : -POS_INF;
    }

    thread_local std::array<EvalCacheEntry, EVAL_CACHE_SIZE> evalCache{};

    const uint64_t evalCacheKey = board.getHash();
    EvalCacheEntry& cacheEntry = evalCache[(evalCacheKey * 0xBF58476D1CE4E5B9ULL) & EVAL_CACHE_MASK];
    if (cacheEntry.valid && cacheEntry.key == evalCacheKey) [[likely]] {
        return cacheEntry.score;
    }

    int32_t eval = board.getIncrementalMaterialDelta();

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const PhaseInfo phase = classifyPhase(board);

    eval += board.getIncrementalPsqtDelta(phase.isEndgame);

    eval += evalBishopPairBonusCached(board);

    int32_t result;
    if (phase.isEndgame && phase.nonPawnMajors == 0) {
        result = evaluatePawnOnlyEndgamePhase(board, eval, whitePawns, blackPawns);
    } else {
        AttackData attackData[2];
        computeAttackData(attackData, board, occ);

        if (phase.isOpening) {
            result = evaluateOpeningPhase(board, eval, whitePawns, blackPawns, attackData);
        } else if (phase.isEarlyMiddlegame) {
            result = evaluateEarlyMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
        } else if (!phase.isEndgame) {
            result = evaluateMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
        } else {
            result = evaluateEndgamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
        }
    }

    if (phase.isEndgame) {
        result = applyOppColorBishopScaling(board, result);
    }

    // Contract: evaluate() returns a SIDE-TO-MOVE relative score (negamax).
    // Terms are computed white-centric internally; negate once at the boundary
    // for Black. (Cache stores the STM-relative value; the zobrist key already
    // encodes side to move, so W/B-to-move map to different cache slots.)
    if (!whiteToMove) result = -result;

    cacheEntry.key = evalCacheKey;
    cacheEntry.score = result;
    cacheEntry.valid = 1;
    return result;
}

} // namespace engine
