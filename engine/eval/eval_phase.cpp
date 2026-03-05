#include "evaluator.hpp"

namespace engine {

namespace {

struct EvalCacheEntry {
    uint64_t key = std::numeric_limits<uint64_t>::max();
    int64_t score = 0;
    uint8_t valid = 0;
};

static constexpr size_t EVAL_CACHE_SIZE = 1u << 9; // 512 entries (~8 KiB), L1-friendly.
static constexpr uint64_t EVAL_CACHE_MASK = static_cast<uint64_t>(EVAL_CACHE_SIZE - 1u);

} // namespace

int64_t Evaluator::evaluateOpeningPhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopmentCached(b);
    eval += evalEarlyQueenCached(b);
    eval += evalCastlingBonusCached(b);
    eval += evalHangingPieces(b, data);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalPieceCoordinationCached(b);
    eval += evalOutpostsCached(b);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalMobility(data);
    eval += (evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data) * engine::KING_SAFETY_OPENING_SCALE_PERCENT) / 100;
    eval += Evaluator::evalInitiative(b, false);
    eval += evalBlockedPawnByBishopsCached(b);

    return eval;
}

int64_t Evaluator::evaluateEarlyMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopmentCached(b);
    eval += evalCastlingBonusCached(b);
    eval += evalHangingPieces(b, data);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalMobility(data);
    eval += evalOutpostsCached(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishopsCached(b);

    return eval;
}

int64_t Evaluator::evaluateMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalHangingPieces(b, data);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalBlockedCenterWithPieces(b, occ);
    eval += evalMobility(data);
    eval += evalPieceCoordinationCached(b);
    eval += evalOutpostsCached(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalKingSafetyWithAttackData(b, whitePawns, blackPawns, data);
    eval += evalKingActivity(b, false);
    eval += evalCastlingBonusCached(b);
    eval += evalKingAttackZone(b, data);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishopsCached(b);

    return eval;
}

int64_t Evaluator::evaluateEndgamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalHangingPieces(b, data);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, true);
    eval += evalKingActivity(b, true);
    eval += evalEndgameKingActivity(b);
    eval += evalMobility(data);
    eval += evalTrappedPieces(b, occ);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalRookEndgamePressure(b);
    eval += evalQueenEndgamePressure(b);
    eval += evalDoubleRookEndgame(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, true);

    return eval;
}

int64_t Evaluator::evaluateCheckmate(const chess::Board& board) noexcept {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

int64_t Evaluator::evaluate(const chess::Board& board) noexcept {
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

    int64_t eval = board.getIncrementalMaterialDelta();

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const int fullMoves = board.getFullMoveClock();

    const int nonPawnMajors = __builtin_popcountll(board.knights_bb[0] | board.knights_bb[1] |
                                             board.bishops_bb[0] | board.bishops_bb[1] |
                                             board.rooks_bb[0]   | board.rooks_bb[1]   |
                                             board.queens_bb[0]  | board.queens_bb[1]);

    constexpr int OPENING_MOVES = 8;
    constexpr int EARLY_MG_MOVES = 15;
    constexpr int PIECE_ENDGAME_THRESHOLD = 5;

    const bool isEndgame = (nonPawnMajors <= PIECE_ENDGAME_THRESHOLD);
    const bool isOpening = !isEndgame && (fullMoves < OPENING_MOVES);
    const bool isEarlyMiddlegame = !isEndgame && !isOpening && (fullMoves < EARLY_MG_MOVES);

    eval += board.getIncrementalPsqtDelta(isEndgame);

    eval += evalBishopPairBonusCached(board);

    AttackData attackData[2];
    computeAttackData(attackData, board, occ);

    int64_t result = eval;
    if (isOpening) {
        result = evaluateOpeningPhase(board, eval, whitePawns, blackPawns, attackData);
    } else if (isEarlyMiddlegame) {
        result = evaluateEarlyMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    } else if (!isEndgame) {
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
