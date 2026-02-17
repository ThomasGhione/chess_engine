#include "evaluator.hpp"
#include "../piecevaluetables.hpp"

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
        if (whiteFileCounts[f] > 1) score += (whiteFileCounts[f] - 1) * engine::DOUBLED_PAWN_PENALTY;
        if (blackFileCounts[f] > 1) score -= (blackFileCounts[f] - 1) * engine::DOUBLED_PAWN_PENALTY;
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
            score += engine::ISOLATED_PAWN_PENALTY;
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
                    score += engine::ISOLATED_PAWN_PENALTY / 2;  // Lighter penalty than isolated (they have adjacent pawns)
                }
            }
        }
        
        // Passed pawn check (no enemy pawns in front on same/adjacent files)
        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = WHITE_FORWARD_FILL[sq];
        if ((blackPawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score += engine::PASSED_PAWN_BONUS;
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
                score -= engine::PASSED_PAWN_BONUS / 2;
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
            score -= engine::ISOLATED_PAWN_PENALTY;
        }
        
        // Pawn chain bonus: supported pawn (has pawn protecting it diagonally behind)
        // Black pawns: "behind" = lower rank (protected from diagonal behind)
        {
            const bool protectedByLeft = (file > 0 && sq >= 7 && (blackPawns & chess::Board::bitMask(static_cast<uint8_t>(sq - 7))));   // Behind-left
            const bool protectedByRight = (file < 7 && sq >= 9 && (blackPawns & chess::Board::bitMask(static_cast<uint8_t>(sq - 9))));  // Behind-right
            
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
                const bool hasSupport = (file > 0 && sq >= 7 && (blackPawns & chess::Board::bitMask(static_cast<uint8_t>(sq - 7))) != 0) ||  // Behind-left
                                       (file < 7 && sq >= 9 && (blackPawns & chess::Board::bitMask(static_cast<uint8_t>(sq - 9))) != 0);     // Behind-right
                
                if (!hasSupport) {
                    score -= engine::ISOLATED_PAWN_PENALTY / 2;  // Lighter penalty than isolated
                }
            }
        }
        
        // Passed pawn check
        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = BLACK_FORWARD_FILL[sq];
        if ((whitePawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score -= engine::PASSED_PAWN_BONUS;
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
                score += engine::PASSED_PAWN_BONUS / 2;
            }
            if (isEndgame) {
                score -= (rank - 1) * 4;
            }
        }
    }
    
    return score;
}

} // namespace engine
