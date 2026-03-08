#include "../evaluator.hpp"

namespace engine {

namespace {

struct EvalCacheEntry {
    uint64_t key = std::numeric_limits<uint64_t>::max();
    int32_t score = 0;
    uint8_t valid = 0;
};

static constexpr size_t EVAL_CACHE_SIZE = 1u << 10; // 1024 entries (~16 KiB), L1-friendly.
static constexpr uint64_t EVAL_CACHE_MASK = static_cast<uint64_t>(EVAL_CACHE_SIZE - 1u);

} // namespace

int32_t Evaluator::evaluateCheckmate(const chess::Board& board) noexcept {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

int32_t Evaluator::evaluate(const chess::Board& board) noexcept {
    const uint8_t activeColor = board.getActiveColor();
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0) [[unlikely]] {
        return (activeColor == chess::Board::BLACK) ? POS_INF : NEG_INF;
    }

    thread_local std::array<EvalCacheEntry, EVAL_CACHE_SIZE> evalCache{};

    const uint64_t fullMoveTag = static_cast<uint64_t>(board.getFullMoveClock());
    const uint64_t evalCacheKey = board.getHash() ^ (fullMoveTag * 0x9E3779B97F4A7C15ULL);
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

    AttackData attackData[2];
    computeAttackData(attackData, board, occ);

    int32_t result = eval;
    if (phase.isOpening) {
        result = evaluateOpeningPhase(board, eval, whitePawns, blackPawns, attackData);
    } else if (phase.isEarlyMiddlegame) {
        result = evaluateEarlyMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    } else if (!phase.isEndgame) {
        result = evaluateMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    } else {
        result = evaluateEndgamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    }

    cacheEntry.key = evalCacheKey;
    cacheEntry.score = result;
    cacheEntry.valid = 1;
    return result;
}

} // namespace engine
