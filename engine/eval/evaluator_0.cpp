#include "evaluator.hpp"
#include "../piecevaluetables.hpp"

namespace engine {

int64_t Evaluator::evaluate(const chess::Board& board) noexcept {
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0 || board.isCheckmate(board.getActiveColor())) [[unlikely]] {
        //(missing king)
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

    // ===================================================
    // GAME PHASE DETECTION
    // ===================================================
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

    // ===================================================
    // PIECE-SQUARE TABLES (always evaluated)
    // ===================================================
    addPsqt(board.pawns_bb[0], board.pawns_bb[1], (isEndgame ? PAWN_END_GAME_VALUES_TABLE : PAWN_VALUES_TABLE).data(), eval);
    addPsqt(board.knights_bb[0], board.knights_bb[1], KNIGHT_VALUES_TABLE.data(), eval);
    addPsqt(board.bishops_bb[0], board.bishops_bb[1], BISHOP_VALUES_TABLE.data(), eval);
    addPsqt(board.rooks_bb[0],   board.rooks_bb[1],   ROOK_VALUES_TABLE.data(), eval);
    addPsqt(board.queens_bb[0],  board.queens_bb[1],  QUEEN_VALUES_TABLE.data(), eval);
    addPsqt(board.kings_bb[0],   board.kings_bb[1],   (isEndgame ? KING_END_GAME_VALUES_TABLE : KING_MIDDLE_GAME_VALUES_TABLE).data(), eval);

    // ===================================================
    // BISHOP PAIR BONUS (always evaluated, all phases)
    // ===================================================
    if (__builtin_popcountll(board.bishops_bb[0]) >= 2) eval += BISHOP_PAIR_BONUS;
    if (__builtin_popcountll(board.bishops_bb[1]) >= 2) eval -= BISHOP_PAIR_BONUS;

    // ===================================================
    // LAZY ATTACK DATA (computed only when needed)
    // OPTIMIZATION: Initialize with isComputed=false, compute on-demand
    // ===================================================
    AttackData attackData[2] = {};  // Zero-initialize (isComputed = false)
    ensureAttackData(attackData, board, occ);
    // ===================================================
    // OPENING PHASE (moves 1-10)
    // Focus: development, king safety, avoid early mistakes
    // ===================================================
    if (isOpening) {
        // CRITICAL: Incentivare sviluppo dei pezzi minori!
        eval += evalMinorPieceDevelopment(board);
        
        // Development penalties (re e torre non sviluppati)
        eval += evalEarlyQueen(board);
        
        // Castling is CRITICAL in opening
        eval += evalCastlingBonus(board);
        
        // Basic piece safety (avoid hanging pieces) - NEEDS attackData
        
        eval += evalHangingPieces(board, attackData);
        
        // Center control è FONDAMENTALE in opening
        eval += evalCentralControl(whitePawns, blackPawns);
        
        // Penalize non-coordinated minor pieces (promote piece coordination)
        eval += evalPieceCoordination(board);
        // Outposts: reward stable knights/bishops supported by pawns and not attacked by enemy pawns
        eval += evalOutposts(board);
        
        // Basic pawn structure (non troppo dettagliato)
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        
        // Mobility bonus (sviluppare pezzi = più mosse) - NEEDS attackData
        eval += evalMobility(attackData);
        
        // Initiative bonus (side to move advantage)
        eval += Evaluator::evalInitiative(board, false);
        
        // Penalize bishops that block pawns directly (opening)
        eval += evalBlockedPawnByBishops(board);

        return eval;
    }
    
    // ===================================================
    // EARLY MIDDLEGAME (moves 10-15)
    // Transition phase: continue development, prepare attacks
    // ===================================================
    if (isEarlyMiddlegame) {
        // Continua a incentivare sviluppo
        eval += evalMinorPieceDevelopment(board);
        
        // Castling still important
        eval += evalCastlingBonus(board);
        
        // Full tactical evaluation - NEEDS attackData
        eval += evalHangingPieces(board, attackData);
        eval += evalTrappedPieces(board, occ);
        
        // Pawn structure becomes more important
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        eval += evalCentralControl(whitePawns, blackPawns);
        
        // Piece activity - NEEDS attackData
        eval += evalMobility(attackData);
        eval += evalOutposts(board);
        eval += evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        
        // King safety starts to matter
        eval += evalKingSafety(board, whitePawns, blackPawns);
        
        // NOTE: King attack zone bonus NOT applied in early middlegame
        // We want pieces to be developed first, not rush attacks prematurely
        
        // Rook evaluation
        eval += evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        
        // Initiative
        eval += evalInitiative(board, false);
        
        // Penalize bishops that block pawns in early middlegame
        eval += evalBlockedPawnByBishops(board);

        return eval;
    }
    
    // ===================================================
    // MIDDLEGAME (moves 15+, many pieces on board)
    // Focus: tactics, king safety, piece coordination
    // ===================================================
    if (isMiddlegame) {
        // Full tactical evaluation (molto importante!) - NEEDS attackData
        eval += evalHangingPieces(board, attackData);
        eval += evalTrappedPieces(board, occ);
        
        // Pawn structure evaluation
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        eval += evalCentralControl(whitePawns, blackPawns);
        eval += evalBlockedCenterWithPieces(board, occ);
        
        // Piece activity and positioning - NEEDS attackData
        eval += evalMobility(attackData);
        eval += evalPieceCoordination(board);
        eval += evalOutposts(board);
        eval += evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        
        // King safety è CRITICO in middlegame
        eval += evalKingSafety(board, whitePawns, blackPawns);
        eval += evalKingActivity(board, false);
        eval += evalCastlingBonus(board);
        
        // King attack zone: reward building multi-piece attacks on enemy king
        // This incentivizes coordinated attacks over repetitive checks
        eval += evalKingAttackZone(board, attackData);
        
        // Rook evaluation (open files, 7th rank)
        eval += evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        // REMOVED: evalPassiveRooks() - double counts with evalRooks() and evalTrappedPieces()
        
        // Initiative
        eval += evalInitiative(board, false);
        
        // Penalize bishops that block pawns in middlegame
        eval += evalBlockedPawnByBishops(board);

        return eval;
    }
    
    // ===================================================
    // ENDGAME (few pieces left)
    // Focus: pawn promotion, king activity, passed pawns
    // ===================================================
    // isEndgame
    // Tactical safety still matters - NEEDS attackData
    eval += evalHangingPieces(board, attackData);
    
    // Pawn structure è CRITICO in endgame
    eval += evalPawnStructure(whitePawns, blackPawns, true);
    
    // King activity è fondamentale
    eval += evalKingActivity(board, true);
    eval += evalEndgameKingActivity(board);
    
    // Piece mobility - NEEDS attackData
    eval += evalMobility(attackData);
    eval += evalTrappedPieces(board, occ);
    
    // Rook evaluation (still important in endgame)
    eval += evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
    
    // Endgame mating bonuses: push opponent king to edge for checkmate
    eval += evalRookEndgamePressure(board);
    eval += evalQueenEndgamePressure(board);
    eval += evalDoubleRookEndgame(board);
    
    // Minor piece positioning
    eval += evalBadBishop(board.bishops_bb[0], whitePawns, 0);
    eval += evalBadBishop(board.bishops_bb[1], blackPawns, 1);
    
    // Initiative (meno importante in endgame)
    eval += evalInitiative(board, true);

    return eval;
}

} // namespace engine
