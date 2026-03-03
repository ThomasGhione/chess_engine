#include "evaluator.hpp"
#include "../piecevaluetables.hpp"

namespace engine {

inline int64_t Evaluator::evaluateOpeningPhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopmentCached(b);
    eval += evalEarlyQueenCached(b);
    eval += evalCastlingBonusCached(b);
    eval += evalHangingPieces(b, data);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalPieceCoordinationCached(b);
    eval += evalOutpostsCached(b);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalMobility(data);
    eval += (evalKingSafety(b, whitePawns, blackPawns) * engine::KING_SAFETY_OPENING_SCALE_PERCENT) / 100;
    eval += Evaluator::evalInitiative(b, false);
    eval += evalBlockedPawnByBishopsCached(b);

    return eval;
}

inline int64_t Evaluator::evaluateEarlyMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopmentCached(b);
    eval += evalCastlingBonusCached(b);
    eval += evalHangingPieces(b, data);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalMobility(data);
    eval += evalOutpostsCached(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalKingSafety(b, whitePawns, blackPawns);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishopsCached(b);

    return eval;
}

inline int64_t Evaluator::evaluateMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalHangingPieces(b, data);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructureCached(b, whitePawns, blackPawns, false);
    eval += evalCentralControlCached(b, whitePawns, blackPawns);
    eval += evalBlockedCenterWithPieces(b, occ);
    eval += evalMobility(data);
    eval += evalPieceCoordinationCached(b);
    eval += evalOutpostsCached(b);
    eval += evalBadBishopCached(b, whitePawns, blackPawns);
    eval += evalKingSafety(b, whitePawns, blackPawns);
    eval += evalKingActivity(b, false);
    eval += evalCastlingBonusCached(b);
    eval += evalKingAttackZone(b, data);
    eval += evalRooksCached(b, whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishopsCached(b);

    return eval;
}

inline int64_t Evaluator::evaluateEndgamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
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
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0 || board.isCheckmate(activeColor)) [[unlikely]] {
        return (activeColor == chess::Board::BLACK) ? POS_INF : NEG_INF;
    }

    int64_t eval = getMaterialDeltaCached(board);

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

    addPsqt(board.pawns_bb[0], board.pawns_bb[1], (isEndgame ? PAWN_END_GAME_VALUES_TABLE : PAWN_VALUES_TABLE).data(), eval);
    addPsqt(board.knights_bb[0], board.knights_bb[1], engine::KNIGHT_VALUES_TABLE.data(), eval);
    addPsqt(board.bishops_bb[0], board.bishops_bb[1], engine::BISHOP_VALUES_TABLE.data(), eval);
    addPsqt(board.rooks_bb[0],   board.rooks_bb[1],   engine::ROOK_VALUES_TABLE.data(), eval);
    addPsqt(board.queens_bb[0],  board.queens_bb[1],  engine::QUEEN_VALUES_TABLE.data(), eval);
    addPsqt(board.kings_bb[0],   board.kings_bb[1],   (isEndgame ? engine::KING_END_GAME_VALUES_TABLE : engine::KING_MIDDLE_GAME_VALUES_TABLE).data(), eval);

    eval += evalBishopPairBonusCached(board);

    AttackData attackData[2] = {};
    ensureAttackData(attackData, board, occ);

    if (isOpening) {
        return evaluateOpeningPhase(board, eval, whitePawns, blackPawns, attackData);
    }

    if (isEarlyMiddlegame) {
        return evaluateEarlyMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    }

    if (!isEndgame) {
        return evaluateMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    }

    return evaluateEndgamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
}

} // namespace engine
