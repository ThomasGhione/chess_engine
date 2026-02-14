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

int64_t Evaluator::evaluate(const chess::Board& board) noexcept {
    //(missing king)
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0 || board.isCheckmate(board.getActiveColor())) [[unlikely]] {
        return evaluateCheckmate(board);
    }

    int64_t eval = getMaterialDelta(board);

    // NOTE: Stalemate is NOT checked here because evaluate() is called AFTER we know
    // there are legal moves (via generateLegalMoves check in searchPosition).
    // Stalemate detection happens in searchPosition() when moves.is_empty()

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const int fullMoves = board.getFullMoveClock();

    // GAME PHASE DETECTION
    const int nonPawnMajors = __builtin_popcountll(board.knights_bb[0] | board.knights_bb[1] |
                                             board.bishops_bb[0] | board.bishops_bb[1] |
                                             board.rooks_bb[0]   | board.rooks_bb[1]   |
                                             board.queens_bb[0]  | board.queens_bb[1]);
    
    // Game phase thresholds
    constexpr int OPENING_MOVES = 10;      // prime 10 mosse = apertura
    constexpr int EARLY_MG_MOVES = 15;     // mosse 10-15 = early middlegame
    constexpr int PIECE_ENDGAME_THRESHOLD = 5;  // TUNED: was 8 (too high, triggered endgame too early)
    
    const bool isEndgame = (nonPawnMajors <= PIECE_ENDGAME_THRESHOLD);
    const bool isOpening = !isEndgame && (fullMoves < OPENING_MOVES);
    const bool isEarlyMiddlegame = !isEndgame && !isOpening && (fullMoves < EARLY_MG_MOVES);
    const bool isMiddlegame = !isEndgame && !isOpening && !isEarlyMiddlegame;

    // PIECE-SQUARE TABLES (always evaluated)
    addPsqt(board.pawns_bb[0], board.pawns_bb[1], (isEndgame ? PAWN_END_GAME_VALUES_TABLE : PAWN_VALUES_TABLE).data(), eval);
    addPsqt(board.knights_bb[0], board.knights_bb[1], KNIGHT_VALUES_TABLE.data(), eval);
    addPsqt(board.bishops_bb[0], board.bishops_bb[1], BISHOP_VALUES_TABLE.data(), eval);
    addPsqt(board.rooks_bb[0],   board.rooks_bb[1],   ROOK_VALUES_TABLE.data(), eval);
    addPsqt(board.queens_bb[0],  board.queens_bb[1],  QUEEN_VALUES_TABLE.data(), eval);
    addPsqt(board.kings_bb[0],   board.kings_bb[1],   (isEndgame ? KING_END_GAME_VALUES_TABLE : KING_MIDDLE_GAME_VALUES_TABLE).data(), eval);

    // BISHOP PAIR BONUS (always evaluated, all phases)
    if (__builtin_popcountll(board.bishops_bb[0]) >= 2) eval += BISHOP_PAIR_BONUS;
    if (__builtin_popcountll(board.bishops_bb[1]) >= 2) eval -= BISHOP_PAIR_BONUS;

    // LAZY ATTACK DATA (computed only when needed)
    AttackData attackData[2] = {};  // Zero-initialize (isComputed = false)
    ensureAttackData(attackData, board, occ);

    // OPENING PHASE (moves 1-10)
    if (isOpening) {
        return evaluateOpeningPhase(board, eval, whitePawns, blackPawns, attackData);
    }
    
    // EARLY MIDDLEGAME (moves 10-15)
    if (isEarlyMiddlegame) {
        return evaluateEarlyMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    }
    
    // MIDDLEGAME (moves 15+, many pieces on board)
    if (isMiddlegame) {
        return evaluateMiddlegamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
    }
    
    // ENDGAME (few pieces left)
    return evaluateEndgamePhase(board, eval, whitePawns, blackPawns, occ, attackData);
}

} // namespace engine
