#include "evaluator.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace engine {

void Evaluator::traceTerm(int32_t& eval, int32_t delta, const char* label) noexcept {
    eval += delta;
    std::cout << "  [TRACE] +" << label << ": " << eval << " (delta=" << delta << ")" << std::endl;
}

int32_t Evaluator::evaluateTrace(const chess::Board& board) noexcept {
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0) [[unlikely]] {
        return evaluateCheckmate(board);
    }

    int32_t eval = board.getIncrementalMaterialDelta();
    int32_t prev = eval;
    std::cout << "  [TRACE] material: " << eval << std::endl;

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const PhaseInfo phase = classifyPhase(board);
    const bool isMiddlegame = !phase.isEndgame && !phase.isOpening && !phase.isEarlyMiddlegame;
    const char* phaseLabel = phase.isEndgame ? "ENDGAME" : phase.isOpening ? "OPENING" : phase.isEarlyMiddlegame ? "EARLY_MG" : "MIDDLEGAME";
    std::cout << "  [TRACE] phase: " << phaseLabel << " (nonPawnMajors=" << phase.nonPawnMajors
              << ", fullMoves=" << phase.fullMoves << ")" << std::endl;

    eval += board.getIncrementalPsqtDelta(phase.isEndgame);
    std::cout << "  [TRACE] +PSQT: " << eval << " (delta=" << (eval-prev) << ")" << std::endl; prev = eval;

    if (((board.bishops_bb[0] & (board.bishops_bb[0] - 1)) != 0ULL)) eval += engine::BISHOP_PAIR_BONUS;
    if (((board.bishops_bb[1] & (board.bishops_bb[1] - 1)) != 0ULL)) eval -= engine::BISHOP_PAIR_BONUS;
    std::cout << "  [TRACE] +bishopPair: " << eval << " (delta=" << (eval-prev) << ")" << std::endl; prev = eval;

    AttackData attackData[2] = {};
    ensureAttackData(attackData, board, occ);

    // Evaluate using the SAME phase-specific terms as evaluate()
    if (phase.isOpening) {
        traceTerm(eval, evalMinorPieceDevelopment(board), "minorDev");
        traceTerm(eval, evalEarlyQueen(board), "earlyQueen");
        traceTerm(eval, evalCastlingBonus(board), "castling");
        traceTerm(eval, evalHangingPieces(board, attackData), "hangingPieces");
        traceTerm(eval, evalCentralControl(whitePawns, blackPawns), "centralControl");
        traceTerm(eval, evalPieceCoordination(board), "coordination");
        traceTerm(eval, evalOutposts(board), "outposts");
        traceTerm(eval, evalPawnStructure(whitePawns, blackPawns, false), "pawnStructure");
        traceTerm(eval, evalMobility(attackData), "mobility");
        traceTerm(eval,
                  (evalKingSafety(board, whitePawns, blackPawns) * engine::KING_SAFETY_OPENING_SCALE_PERCENT) / 100,
                  "kingSafetyOpening");
        traceTerm(eval, evalInitiative(board, false), "initiative");
        traceTerm(eval, evalBlockedPawnByBishops(board), "blockedPawnBishops");
    } else if (phase.isEarlyMiddlegame) {
        traceTerm(eval, evalMinorPieceDevelopment(board), "minorDev");
        traceTerm(eval, evalCastlingBonus(board), "castling");
        traceTerm(eval, evalHangingPieces(board, attackData), "hangingPieces");
        traceTerm(eval, evalTrappedPieces(board, occ), "trappedPieces");
        traceTerm(eval, evalPawnStructure(whitePawns, blackPawns, false), "pawnStructure");
        traceTerm(eval, evalCentralControl(whitePawns, blackPawns), "centralControl");
        traceTerm(eval, evalMobility(attackData), "mobility");
        traceTerm(eval, evalOutposts(board), "outposts");
        traceTerm(eval, evalBadBishop(board.bishops_bb[0], whitePawns, 0), "badBishopW");
        traceTerm(eval, evalBadBishop(board.bishops_bb[1], blackPawns, 1), "badBishopB");
        traceTerm(eval, evalKingSafety(board, whitePawns, blackPawns), "kingSafety");
        traceTerm(eval, evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns), "rooks");
        traceTerm(eval, evalInitiative(board, false), "initiative");
        traceTerm(eval, evalBlockedPawnByBishops(board), "blockedPawnBishops");
    } else if (isMiddlegame) {
        traceTerm(eval, evalHangingPieces(board, attackData), "hangingPieces");
        traceTerm(eval, evalTrappedPieces(board, occ), "trappedPieces");
        traceTerm(eval, evalPawnStructure(whitePawns, blackPawns, false), "pawnStructure");
        traceTerm(eval, evalCentralControl(whitePawns, blackPawns), "centralControl");
        traceTerm(eval, evalBlockedCenterWithPieces(board, occ), "blockedCenter");
        traceTerm(eval, evalMobility(attackData), "mobility");
        traceTerm(eval, evalPieceCoordination(board), "coordination");
        traceTerm(eval, evalOutposts(board), "outposts");
        traceTerm(eval, evalBadBishop(board.bishops_bb[0], whitePawns, 0), "badBishopW");
        traceTerm(eval, evalBadBishop(board.bishops_bb[1], blackPawns, 1), "badBishopB");
        traceTerm(eval, evalKingSafety(board, whitePawns, blackPawns), "kingSafety");
        traceTerm(eval, evalKingActivity(board, false), "kingActivity");
        traceTerm(eval, evalCastlingBonus(board), "castling");
        traceTerm(eval, evalKingAttackZone(board, attackData), "kingAttackZone");
        traceTerm(eval, evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns), "rooks");
        traceTerm(eval, evalInitiative(board, false), "initiative");
        traceTerm(eval, evalBlockedPawnByBishops(board), "blockedPawnBishops");
    } else {
        // Endgame
        traceTerm(eval, evalHangingPieces(board, attackData), "hangingPieces");
        traceTerm(eval, evalPawnStructure(whitePawns, blackPawns, true), "pawnStructure");
        traceTerm(eval, evalKingActivity(board, true), "kingActivity");
        traceTerm(eval, evalEndgameKingActivity(board), "endgameKingActivity");
        traceTerm(eval, evalMobility(attackData), "mobility");
        traceTerm(eval, evalTrappedPieces(board, occ), "trappedPieces");
        traceTerm(eval, evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns), "rooks");
        traceTerm(eval, evalRookEndgamePressure(board), "rookEgPressure");
        traceTerm(eval, evalQueenEndgamePressure(board), "queenEgPressure");
        traceTerm(eval, evalDoubleRookEndgame(board), "doubleRookEg");
        traceTerm(eval, evalBadBishop(board.bishops_bb[0], whitePawns, 0), "badBishopW");
        traceTerm(eval, evalBadBishop(board.bishops_bb[1], blackPawns, 1), "badBishopB");
        traceTerm(eval, evalInitiative(board, true), "initiative");
    }

    std::cout << "  [TRACE] TOTAL: " << eval << std::endl;
    return eval;
}

} // namespace engine
