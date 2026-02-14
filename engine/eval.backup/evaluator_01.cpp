#include "evaluator.hpp"
#include "../piecevaluetables.hpp"
#include <algorithm>
#include <cstring>
namespace engine {



const std::array<uint64_t, 8> Evaluator::FILE_MASKS = []() constexpr {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        masks[f] = 0x0101010101010101ULL << f;
    }
    return masks;
}();

const std::array<uint64_t, 8> Evaluator::ADJACENT_FILES_ONLY = []() constexpr {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        uint64_t m = 0;
        if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
        if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
        masks[f] = m;
    }
    return masks;
}();

const std::array<uint64_t, 8> Evaluator::ADJACENT_AND_FILE_MASKS = []() constexpr {
    std::array<uint64_t, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        uint64_t m = (0x0101010101010101ULL << f);
        if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
        if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
        masks[f] = m;
    }
    return masks;
}();

const std::array<uint64_t, 64> Evaluator::KING_PROXIMITY_MASKS = []() constexpr {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) {
        uint64_t mask = 0;
        const int rank = chess::Board::rankOf(sq);
        const int file = chess::Board::fileOf(sq);

        for (int r = std::max(0, rank - 2); r <= std::min(7, rank + 2); ++r) {
            for (int f = std::max(0, file - 2); f <= std::min(7, file + 2); ++f) {
                const int target = (r << 3) | f;
                const int dist = std::abs(r - rank) + std::abs(f - file);
                if (dist <= 2 && target != sq) {
                    mask |= chess::Board::bitMask(target);
                }
            }
        }
        masks[sq] = mask;
    }
    return masks;
}();

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

void Evaluator::addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int64_t* table, int64_t& eval) noexcept {
    while (bbWhite) {
        const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbWhite));
        bbWhite &= (bbWhite - 1);
        eval += table[sq];
    }
    while (bbBlack) {
        const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbBlack));
        bbBlack &= (bbBlack - 1);
        const uint8_t idx = mirrorIndex(sq);
        eval -= table[idx];
    }
}

int64_t Evaluator::evalInitiative(const chess::Board& b, bool isEndgame) noexcept {
    return isEndgame
        ? evalInitiativeImpl<true>(b.getActiveColor())
        : evalInitiativeImpl<false>(b.getActiveColor());
}

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

int64_t Evaluator::evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    int64_t score = 0;
    
    // Per-file pawn counts for doubled pawn detection (vectorizable)
    int whiteFileCounts[8] = {};
    int blackFileCounts[8] = {};
    
    // Count pawns per file (SIMD-friendly loop)
    for (int f = 0; f < 8; ++f) {
        const uint64_t fm = FILE_MASKS[f];
        whiteFileCounts[f] = __builtin_popcountll(whitePawns & fm);
        blackFileCounts[f] = __builtin_popcountll(blackPawns & fm);
    }
    
    // Score doubled pawns (vectorizable)
    for (int f = 0; f < 8; ++f) {
        if (whiteFileCounts[f] > 1) score += (whiteFileCounts[f] - 1) * DOUBLED_PAWN_PENALTY;
        if (blackFileCounts[f] > 1) score -= (blackFileCounts[f] - 1) * DOUBLED_PAWN_PENALTY;
    }
    
    // Evaluate WHITE pawns (isolated + passed in ONE loop)
    uint64_t wp = whitePawns;
    while (wp) {
        const int sq = popLSB(wp);
        const int file = chess::Board::fileOf(sq);
        const int rank = chess::Board::rankOf(sq);
        
        // Isolated pawn check (no friendly pawns on adjacent files) - OPTIMIZED
        const uint64_t adjFilesMask = ADJACENT_FILES_ONLY[file];
        if ((whitePawns & adjFilesMask) == 0) [[unlikely]] {
            score += ISOLATED_PAWN_PENALTY;
        }
        
        // Pawn chain bonus: supported pawn (has pawn protecting it diagonally behind)
        // White pawns: "behind" = higher rank (protected from diagonal behind)
        {
            const bool protectedByLeft = (file > 0 && (whitePawns & chess::Board::bitMask(sq + 7)));   // Behind-left
            const bool protectedByRight = (file < 7 && (whitePawns & chess::Board::bitMask(sq + 9)));  // Behind-right
            
            if (protectedByLeft || protectedByRight) {
                score += 15;  // Bonus for supported pawns (chain bonus helps with tactics)
            }
        }
        
        // Backward pawn check: pawn can't advance and no supporting pawns
        // A backward pawn is blocked (pawn in front) and no support from behind
        // White pawns move down (decreasing rank: 6→0), so "behind" = higher rank
        {
            const int forwardSq = sq - 8;  // Square in front (lower rank)
            const bool isBlocked = (forwardSq >= 0) && ((whitePawns | blackPawns) & chess::Board::bitMask(forwardSq));
            
            if (isBlocked) {
                // Check for supporting pawns (diagonally behind on adjacent files)
                const bool hasSupport = (file > 0 && (whitePawns & chess::Board::bitMask(sq + 7)) != 0) ||  // Behind-left
                                       (file < 7 && (whitePawns & chess::Board::bitMask(sq + 9)) != 0);     // Behind-right
                
                if (!hasSupport) {
                    score += ISOLATED_PAWN_PENALTY / 2;  // Lighter penalty than isolated (they have adjacent pawns)
                }
            }
        }
        
        // Passed pawn check (no enemy pawns in front on same/adjacent files)
        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = WHITE_FORWARD_FILL[sq];
        if ((blackPawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score += PASSED_PAWN_BONUS;
            // Slight bonus for advancement even in middlegame
            const int advancement = 6 - rank; // rank 6 (start) -> 0, rank 1 (near promo) -> 5
            score += advancement * (isEndgame ? 6 : 2);

            // Extra danger on 7th rank (one step from promotion)
            if (rank == 1) {
                score += isEndgame ? 40 : 20;
            }

            // If blocked by an enemy pawn directly in front, reduce the bonus
            const int forwardSq = sq - 8;
            if (forwardSq >= 0 && (blackPawns & chess::Board::bitMask(forwardSq))) {
                score -= PASSED_PAWN_BONUS / 2;
            }
            if (isEndgame) {
                score += (6 - rank) * 4; // Reduced from 6
            }
        }
    }
    
    // Evaluate BLACK pawns (isolated + passed in ONE loop)
    uint64_t bp = blackPawns;
    while (bp) {
        const int sq = popLSB(bp);
        const int file = chess::Board::fileOf(sq);
        const int rank = chess::Board::rankOf(sq);
        
        // Isolated pawn check - OPTIMIZED
        const uint64_t adjFilesMask = ADJACENT_FILES_ONLY[file];
        if ((blackPawns & adjFilesMask) == 0) [[unlikely]] {
            score -= ISOLATED_PAWN_PENALTY;
        }
        
        // Pawn chain bonus: supported pawn (has pawn protecting it diagonally behind)
        // Black pawns: "behind" = lower rank (protected from diagonal behind)
        {
            const bool protectedByLeft = (file > 0 && (blackPawns & chess::Board::bitMask(sq - 7)));   // Behind-left
            const bool protectedByRight = (file < 7 && (blackPawns & chess::Board::bitMask(sq - 9)));  // Behind-right
            
            if (protectedByLeft || protectedByRight) {
                score -= 15;  // Bonus for Black chain (subtract because Black is negative)
            }
        }
        
        // Backward pawn check: pawn can't advance and no supporting pawns
        // Black pawns move up (increasing rank: 1→7), so "behind" = lower rank
        {
            const int forwardSq = sq + 8;  // Square in front (higher rank)
            const bool isBlocked = (forwardSq < 64) && ((whitePawns | blackPawns) & chess::Board::bitMask(forwardSq));
            
            if (isBlocked) {
                // Check for supporting pawns (diagonally behind on adjacent files)
                // FIXED: Corrected file bounds - sq-7 is behind-left (file decreases), sq-9 is behind-right (file increases)
                const bool hasSupport = (file > 0 && (blackPawns & chess::Board::bitMask(sq - 7)) != 0) ||  // Behind-left
                                       (file < 7 && (blackPawns & chess::Board::bitMask(sq - 9)) != 0);     // Behind-right
                
                if (!hasSupport) {
                    score -= ISOLATED_PAWN_PENALTY / 2;  // Lighter penalty than isolated
                }
            }
        }
        
        // Passed pawn check
        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = BLACK_FORWARD_FILL[sq];
        if ((whitePawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score -= PASSED_PAWN_BONUS;
            // Slight bonus for advancement even in middlegame
            const int advancement = rank - 1; // rank 1 (start) -> 0, rank 6 (near promo) -> 5
            score -= advancement * (isEndgame ? 6 : 2);

            // Extra danger on 7th rank (one step from promotion)
            if (rank == 6) {
                score -= isEndgame ? 40 : 20;
            }

            // If blocked by an enemy pawn directly in front, reduce the bonus
            const int forwardSq = sq + 8;
            if (forwardSq < 64 && (whitePawns & chess::Board::bitMask(forwardSq))) {
                score += PASSED_PAWN_BONUS / 2;
            }
            if (isEndgame) {
                score -= (rank - 1) * 4;
            }
        }
    }
    
    return score;
}

} // namespace engine
