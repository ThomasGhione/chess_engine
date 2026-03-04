#include "evaluator.hpp"

namespace engine {

const std::array<uint64_t, 8> Evaluator::FILE_MASKS = Evaluator::initFileMasks();
const std::array<uint64_t, 8> Evaluator::ADJACENT_FILES_ONLY = Evaluator::initAdjacentFilesOnly();
const std::array<uint64_t, 8> Evaluator::ADJACENT_AND_FILE_MASKS = Evaluator::initAdjacentAndFileMasks();
const std::array<uint64_t, 64> Evaluator::KING_PROXIMITY_MASKS = Evaluator::initKingProximityMasks();

const std::array<uint64_t, 64> Evaluator::WHITE_FORWARD_FILL = Evaluator::initWhiteForwardFill();
const std::array<uint64_t, 64> Evaluator::BLACK_FORWARD_FILL = Evaluator::initBlackForwardFill();

int64_t Evaluator::getMaterialDelta(const chess::Board& b) noexcept {
    return static_cast<int64_t>(
          (__builtin_popcountll(b.pawns_bb[0])   - __builtin_popcountll(b.pawns_bb[1]))   * PIECE_VALUES[chess::Board::PAWN]
        + (__builtin_popcountll(b.knights_bb[0]) - __builtin_popcountll(b.knights_bb[1])) * PIECE_VALUES[chess::Board::KNIGHT]
        + (__builtin_popcountll(b.bishops_bb[0]) - __builtin_popcountll(b.bishops_bb[1])) * PIECE_VALUES[chess::Board::BISHOP]
        + (__builtin_popcountll(b.rooks_bb[0])   - __builtin_popcountll(b.rooks_bb[1]))   * PIECE_VALUES[chess::Board::ROOK]
        + (__builtin_popcountll(b.queens_bb[0])  - __builtin_popcountll(b.queens_bb[1]))  * PIECE_VALUES[chess::Board::QUEEN]
        + (__builtin_popcountll(b.kings_bb[0])   - __builtin_popcountll(b.kings_bb[1]))   * PIECE_VALUES[chess::Board::KING]);
}

int64_t Evaluator::getMaterialDeltaCached(const chess::Board& b) noexcept {
    return b.getIncrementalMaterialDelta();
}

int64_t Evaluator::evalPawnStructureCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    if (isEndgame) {
        constexpr uint32_t TERM = chess::Board::EVAL_CACHE_PAWN_STRUCTURE_EG;
        if (b.hasEvalCacheTerm<TERM>()) {
            return b.getEvalCacheTerm<TERM>();
        }
        const int64_t score = evalPawnStructure(whitePawns, blackPawns, true);
        b.setEvalCacheTerm<TERM>(score);
        return score;
    }

    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_PAWN_STRUCTURE_MG;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalPawnStructure(whitePawns, blackPawns, false);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalBishopPairBonusCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_BISHOP_PAIR_BONUS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    int64_t score = 0;
    if ((b.bishops_bb[0] & (b.bishops_bb[0] - 1)) != 0ULL) score += engine::BISHOP_PAIR_BONUS;
    if ((b.bishops_bb[1] & (b.bishops_bb[1] - 1)) != 0ULL) score -= engine::BISHOP_PAIR_BONUS;
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalCastlingBonusCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_CASTLING_BONUS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalCastlingBonus(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalRooksCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_ROOKS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalRooks(b.rooks_bb[0], b.rooks_bb[1], whitePawns, blackPawns);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalBadBishopCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_BAD_BISHOP;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score =
        evalBadBishop(b.bishops_bb[0], whitePawns, 0) +
        evalBadBishop(b.bishops_bb[1], blackPawns, 1);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalBlockedPawnByBishopsCached(const chess::Board& b) noexcept {
    return evalBlockedPawnByBishops(b);
}

int64_t Evaluator::evalMinorPieceDevelopmentCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_MINOR_DEVELOPMENT;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalMinorPieceDevelopment(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalEarlyQueenCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_EARLY_QUEEN;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalEarlyQueen(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalOutpostsCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_OUTPOSTS;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalOutposts(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalPieceCoordinationCached(const chess::Board& b) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_PIECE_COORDINATION;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalPieceCoordination(b);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalCentralControlCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint32_t TERM = chess::Board::EVAL_CACHE_CENTRAL_CONTROL;
    if (b.hasEvalCacheTerm<TERM>()) {
        return b.getEvalCacheTerm<TERM>();
    }
    const int64_t score = evalCentralControl(whitePawns, blackPawns);
    b.setEvalCacheTerm<TERM>(score);
    return score;
}

int64_t Evaluator::evalInitiative(const chess::Board& b, bool isEndgame) noexcept {
    return isEndgame
        ? evalInitiativeImpl<true>(b.getActiveColor())
        : evalInitiativeImpl<false>(b.getActiveColor());
}

} // namespace engine
