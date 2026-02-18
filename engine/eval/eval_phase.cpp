#include "evaluator.hpp"
#include "../piecevaluetables.hpp"

namespace engine {

inline int64_t Evaluator::evaluateOpeningPhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopment(b);
    eval += evalEarlyQueen(b);
    eval += evalCastlingBonus(b);
    eval += evalHangingPieces(b, data);
    eval += evalCentralControl(whitePawns, blackPawns);
    eval += evalPieceCoordination(b);
    eval += evalOutposts(b);
    eval += evalPawnStructure(whitePawns, blackPawns, false);
    eval += evalMobility(data);
    eval += Evaluator::evalInitiative(b, false);
    eval += evalBlockedPawnByBishops(b);

    return eval;
}

inline int64_t Evaluator::evaluateEarlyMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalMinorPieceDevelopment(b);
    eval += evalCastlingBonus(b);
    eval += evalHangingPieces(b, data);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructure(whitePawns, blackPawns, false);
    eval += evalCentralControl(whitePawns, blackPawns);
    eval += evalMobility(data);
    eval += evalOutposts(b);
    eval += evalBadBishop(b.bishops_bb[0], whitePawns, 0);
    eval += evalBadBishop(b.bishops_bb[1], blackPawns, 1);
    eval += evalKingSafety(b, whitePawns, blackPawns);
    eval += evalRooks(b.rooks_bb[0], b.rooks_bb[1], whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishops(b);

    return eval;
}

inline int64_t Evaluator::evaluateMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalHangingPieces(b, data);
    eval += evalTrappedPieces(b, occ);
    eval += evalPawnStructure(whitePawns, blackPawns, false);
    eval += evalCentralControl(whitePawns, blackPawns);
    eval += evalBlockedCenterWithPieces(b, occ);
    eval += evalMobility(data);
    eval += evalPieceCoordination(b);
    eval += evalOutposts(b);
    eval += evalBadBishop(b.bishops_bb[0], whitePawns, 0);
    eval += evalBadBishop(b.bishops_bb[1], blackPawns, 1);
    eval += evalKingSafety(b, whitePawns, blackPawns);
    eval += evalKingActivity(b, false);
    eval += evalCastlingBonus(b);
    eval += evalKingAttackZone(b, data);
    eval += evalRooks(b.rooks_bb[0], b.rooks_bb[1], whitePawns, blackPawns);
    eval += evalInitiative(b, false);
    eval += evalBlockedPawnByBishops(b);

    return eval;
}

inline int64_t Evaluator::evaluateEndgamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept {
    eval += evalHangingPieces(b, data);
    eval += evalPawnStructure(whitePawns, blackPawns, true);
    eval += evalKingActivity(b, true);
    eval += evalEndgameKingActivity(b);
    eval += evalMobility(data);
    eval += evalTrappedPieces(b, occ);
    eval += evalRooks(b.rooks_bb[0], b.rooks_bb[1], whitePawns, blackPawns);
    eval += evalRookEndgamePressure(b);
    eval += evalQueenEndgamePressure(b);
    eval += evalDoubleRookEndgame(b);
    eval += evalBadBishop(b.bishops_bb[0], whitePawns, 0);
    eval += evalBadBishop(b.bishops_bb[1], blackPawns, 1);
    eval += evalInitiative(b, true);

    return eval;
}

int64_t Evaluator::evaluateCheckmate(const chess::Board& board) noexcept {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

int64_t Evaluator::evaluate(const chess::Board& board) noexcept {
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0 || board.isCheckmate(board.getActiveColor())) [[unlikely]] {
        return evaluateCheckmate(board);
    }

    int64_t eval = getMaterialDelta(board);

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const int fullMoves = board.getFullMoveClock();

    const int nonPawnMajors = __builtin_popcountll(board.knights_bb[0] | board.knights_bb[1] |
                                             board.bishops_bb[0] | board.bishops_bb[1] |
                                             board.rooks_bb[0]   | board.rooks_bb[1]   |
                                             board.queens_bb[0]  | board.queens_bb[1]);

    constexpr int OPENING_MOVES = 10;
    constexpr int EARLY_MG_MOVES = 15;
    constexpr int PIECE_ENDGAME_THRESHOLD = 5;

    const bool isEndgame = (nonPawnMajors <= PIECE_ENDGAME_THRESHOLD);
    const bool isOpening = !isEndgame && (fullMoves < OPENING_MOVES);
    const bool isEarlyMiddlegame = !isEndgame && !isOpening && (fullMoves < EARLY_MG_MOVES);
    const bool isMiddlegame = !isEndgame && !isOpening && !isEarlyMiddlegame;

    addPsqt(board.pawns_bb[0], board.pawns_bb[1], (isEndgame ? PAWN_END_GAME_VALUES_TABLE : PAWN_VALUES_TABLE).data(), eval);
    addPsqt(board.knights_bb[0], board.knights_bb[1], engine::KNIGHT_VALUES_TABLE.data(), eval);
    addPsqt(board.bishops_bb[0], board.bishops_bb[1], engine::BISHOP_VALUES_TABLE.data(), eval);
    addPsqt(board.rooks_bb[0],   board.rooks_bb[1],   engine::ROOK_VALUES_TABLE.data(), eval);
    addPsqt(board.queens_bb[0],  board.queens_bb[1],  engine::QUEEN_VALUES_TABLE.data(), eval);
    addPsqt(board.kings_bb[0],   board.kings_bb[1],   (isEndgame ? engine::KING_END_GAME_VALUES_TABLE : engine::KING_MIDDLE_GAME_VALUES_TABLE).data(), eval);

    if (__builtin_popcountll(board.bishops_bb[0]) >= 2) eval += engine::BISHOP_PAIR_BONUS;
    if (__builtin_popcountll(board.bishops_bb[1]) >= 2) eval -= engine::BISHOP_PAIR_BONUS;

    AttackData attackData[2] = {};
    ensureAttackData(attackData, board, occ);

    if (isOpening) {
        return evaluateOpeningPhase(board, eval, whitePawns, blackPawns, attackData);
    }

    if (isEarlyMiddlegame) {
        return evaluateEarlyMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    }

    if (isMiddlegame) {
        return evaluateMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    }

    return evaluateEndgamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
}

} // namespace engine
