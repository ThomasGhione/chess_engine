#include "evaluator.hpp"
#include "../piecevaluetables.hpp"
#include <algorithm>
#include <cstring>
namespace engine {

int64_t Evaluator::evaluateTrace(const chess::Board& board) noexcept {
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0 || board.isCheckmate(board.getActiveColor())) [[unlikely]] {
        return evaluateCheckmate(board);
    }

    int64_t eval = getMaterialDelta(board);
    int64_t prev = eval;
    std::cout << "  [TRACE] material: " << eval << std::endl;

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];
    const int fullMoves = board.getFullMoveClock();

    // Game phase detection (must match evaluate()!)
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

    const char* phase = isEndgame ? "ENDGAME" : isOpening ? "OPENING" : isEarlyMiddlegame ? "EARLY_MG" : "MIDDLEGAME";
    std::cout << "  [TRACE] phase: " << phase << " (nonPawnMajors=" << nonPawnMajors << ", fullMoves=" << fullMoves << ")" << std::endl;

    addPsqt(board.pawns_bb[0], board.pawns_bb[1], (isEndgame ? PAWN_END_GAME_VALUES_TABLE : PAWN_VALUES_TABLE).data(), eval);
    addPsqt(board.knights_bb[0], board.knights_bb[1], KNIGHT_VALUES_TABLE.data(), eval);
    addPsqt(board.bishops_bb[0], board.bishops_bb[1], BISHOP_VALUES_TABLE.data(), eval);
    addPsqt(board.rooks_bb[0],   board.rooks_bb[1],   ROOK_VALUES_TABLE.data(), eval);
    addPsqt(board.queens_bb[0],  board.queens_bb[1],  QUEEN_VALUES_TABLE.data(), eval);
    addPsqt(board.kings_bb[0],   board.kings_bb[1],   (isEndgame ? KING_END_GAME_VALUES_TABLE : KING_MIDDLE_GAME_VALUES_TABLE).data(), eval);
    std::cout << "  [TRACE] +PSQT: " << eval << " (delta=" << (eval-prev) << ")" << std::endl; prev = eval;

    if (__builtin_popcountll(board.bishops_bb[0]) >= 2) eval += BISHOP_PAIR_BONUS;
    if (__builtin_popcountll(board.bishops_bb[1]) >= 2) eval -= BISHOP_PAIR_BONUS;
    std::cout << "  [TRACE] +bishopPair: " << eval << " (delta=" << (eval-prev) << ")" << std::endl; prev = eval;

    AttackData attackData[2] = {};
    ensureAttackData(attackData, board, occ);

    int64_t v;

    // Evaluate using the SAME phase-specific terms as evaluate()
    if (isOpening) {
        v = evalMinorPieceDevelopment(board);
        eval += v; std::cout << "  [TRACE] +minorDev: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalEarlyQueen(board);
        eval += v; std::cout << "  [TRACE] +earlyQueen: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalCastlingBonus(board);
        eval += v; std::cout << "  [TRACE] +castling: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalHangingPieces(board, attackData);
        eval += v; std::cout << "  [TRACE] +hangingPieces: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalCentralControl(whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +centralControl: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalPieceCoordination(board);
        eval += v; std::cout << "  [TRACE] +coordination: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalOutposts(board);
        eval += v; std::cout << "  [TRACE] +outposts: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalPawnStructure(whitePawns, blackPawns, false);
        eval += v; std::cout << "  [TRACE] +pawnStructure: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalMobility(attackData);
        eval += v; std::cout << "  [TRACE] +mobility: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalInitiative(board, false);
        eval += v; std::cout << "  [TRACE] +initiative: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBlockedPawnByBishops(board);
        eval += v; std::cout << "  [TRACE] +blockedPawnBishops: " << eval << " (delta=" << v << ")" << std::endl;
    } else if (isEarlyMiddlegame) {
        v = evalMinorPieceDevelopment(board);
        eval += v; std::cout << "  [TRACE] +minorDev: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalCastlingBonus(board);
        eval += v; std::cout << "  [TRACE] +castling: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalHangingPieces(board, attackData);
        eval += v; std::cout << "  [TRACE] +hangingPieces: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalTrappedPieces(board, occ);
        eval += v; std::cout << "  [TRACE] +trappedPieces: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalPawnStructure(whitePawns, blackPawns, false);
        eval += v; std::cout << "  [TRACE] +pawnStructure: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalCentralControl(whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +centralControl: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalMobility(attackData);
        eval += v; std::cout << "  [TRACE] +mobility: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalOutposts(board);
        eval += v; std::cout << "  [TRACE] +outposts: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += v; std::cout << "  [TRACE] +badBishopW: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        eval += v; std::cout << "  [TRACE] +badBishopB: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalKingSafety(board, whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +kingSafety: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +rooks: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalInitiative(board, false);
        eval += v; std::cout << "  [TRACE] +initiative: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBlockedPawnByBishops(board);
        eval += v; std::cout << "  [TRACE] +blockedPawnBishops: " << eval << " (delta=" << v << ")" << std::endl;
    } else if (isMiddlegame) {
        v = evalHangingPieces(board, attackData);
        eval += v; std::cout << "  [TRACE] +hangingPieces: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalTrappedPieces(board, occ);
        eval += v; std::cout << "  [TRACE] +trappedPieces: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalPawnStructure(whitePawns, blackPawns, false);
        eval += v; std::cout << "  [TRACE] +pawnStructure: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalCentralControl(whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +centralControl: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBlockedCenterWithPieces(board, occ);
        eval += v; std::cout << "  [TRACE] +blockedCenter: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalMobility(attackData);
        eval += v; std::cout << "  [TRACE] +mobility: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalPieceCoordination(board);
        eval += v; std::cout << "  [TRACE] +coordination: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalOutposts(board);
        eval += v; std::cout << "  [TRACE] +outposts: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += v; std::cout << "  [TRACE] +badBishopW: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        eval += v; std::cout << "  [TRACE] +badBishopB: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalKingSafety(board, whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +kingSafety: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalKingActivity(board, false);
        eval += v; std::cout << "  [TRACE] +kingActivity: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalCastlingBonus(board);
        eval += v; std::cout << "  [TRACE] +castling: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalKingAttackZone(board, attackData);
        eval += v; std::cout << "  [TRACE] +kingAttackZone: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +rooks: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalInitiative(board, false);
        eval += v; std::cout << "  [TRACE] +initiative: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBlockedPawnByBishops(board);
        eval += v; std::cout << "  [TRACE] +blockedPawnBishops: " << eval << " (delta=" << v << ")" << std::endl;
    } else {
        // Endgame
        v = evalHangingPieces(board, attackData);
        eval += v; std::cout << "  [TRACE] +hangingPieces: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalPawnStructure(whitePawns, blackPawns, true);
        eval += v; std::cout << "  [TRACE] +pawnStructure: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalKingActivity(board, true);
        eval += v; std::cout << "  [TRACE] +kingActivity: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalEndgameKingActivity(board);
        eval += v; std::cout << "  [TRACE] +endgameKingActivity: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalMobility(attackData);
        eval += v; std::cout << "  [TRACE] +mobility: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalTrappedPieces(board, occ);
        eval += v; std::cout << "  [TRACE] +trappedPieces: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        eval += v; std::cout << "  [TRACE] +rooks: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalRookEndgamePressure(board);
        eval += v; std::cout << "  [TRACE] +rookEgPressure: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalQueenEndgamePressure(board);
        eval += v; std::cout << "  [TRACE] +queenEgPressure: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalDoubleRookEndgame(board);
        eval += v; std::cout << "  [TRACE] +doubleRookEg: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += v; std::cout << "  [TRACE] +badBishopW: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        eval += v; std::cout << "  [TRACE] +badBishopB: " << eval << " (delta=" << v << ")" << std::endl;
        v = evalInitiative(board, true);
        eval += v; std::cout << "  [TRACE] +initiative: " << eval << " (delta=" << v << ")" << std::endl;
    }

    std::cout << "  [TRACE] TOTAL: " << eval << std::endl;
    return eval;
}

} // namespace engine
