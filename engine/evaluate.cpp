#include "engine.hpp"

namespace engine {

int64_t Engine::getMaterialDelta(const chess::Board& b) noexcept {
    return static_cast<int64_t>(
          (__builtin_popcountll(b.pawns_bb[0])   - __builtin_popcountll(b.pawns_bb[1]))   * PIECE_VALUES[chess::Board::PAWN]
        + (__builtin_popcountll(b.knights_bb[0]) - __builtin_popcountll(b.knights_bb[1])) * PIECE_VALUES[chess::Board::KNIGHT]
        + (__builtin_popcountll(b.bishops_bb[0]) - __builtin_popcountll(b.bishops_bb[1])) * PIECE_VALUES[chess::Board::BISHOP]
        + (__builtin_popcountll(b.rooks_bb[0])   - __builtin_popcountll(b.rooks_bb[1]))   * PIECE_VALUES[chess::Board::ROOK]
        + (__builtin_popcountll(b.queens_bb[0])  - __builtin_popcountll(b.queens_bb[1]))  * PIECE_VALUES[chess::Board::QUEEN]
        + (__builtin_popcountll(b.kings_bb[0])   - __builtin_popcountll(b.kings_bb[1]))   * PIECE_VALUES[chess::Board::KING]);
}


int64_t Engine::evaluateCheckmate(const chess::Board& board) noexcept {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

__attribute__((always_inline))
inline void addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int64_t* table, int64_t& eval) noexcept {
    // White pieces: use index as-is
    while (bbWhite) {
        const uint8_t sq = popLSB(bbWhite);
        eval += table[sq];
    }
    // Black pieces: mirror index vertically (inline for performance)
    while (bbBlack) {
        const uint8_t sq = popLSB(bbBlack);
        eval -= table[chess::Board::verticalMirror(sq)];
    }
}

// REMOVED: Using popLSB() instead (defined in board.hpp)
// REMOVED: Using chess::Board::fileMask() instead (defined in board.hpp)

__attribute__((always_inline))
inline uint64_t adjacentFilesMask(int file) noexcept {
    uint64_t m = 0;
    if (file > 0) m |= chess::Board::fileMask(file - 1);
    if (file < 7) m |= chess::Board::fileMask(file + 1);
    return m;
}

// Precompute masks for faster pawn evaluation
namespace {
    // Forward fill masks for passed pawn detection (compile-time constant)
    constexpr std::array<uint64_t, 64> initWhiteForwardFill() {
        std::array<uint64_t, 64> result{};
        for (int sq = 0; sq < 64; ++sq) {
            const int rank = chess::Board::rankOf(sq);
            result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
        }
        return result;
    }
    
    constexpr std::array<uint64_t, 64> initBlackForwardFill() {
        std::array<uint64_t, 64> result{};
        for (int sq = 0; sq < 64; ++sq) {
            const int rank = chess::Board::rankOf(sq);
            result[sq] = (rank > 0) ? ((chess::Board::bitMask(rank * 8)) - 1ULL) : 0ULL;
        }
        return result;
    }
    
    constexpr auto WHITE_FORWARD_FILL = initWhiteForwardFill();
    constexpr auto BLACK_FORWARD_FILL = initBlackForwardFill();
    
    // File masks (already defined in fileMask() but we precalculate for speed)
    constexpr std::array<uint64_t, 8> FILE_MASKS = []() constexpr {
        std::array<uint64_t, 8> masks{};
        for (int f = 0; f < 8; ++f) {
            masks[f] = 0x0101010101010101ULL << f;
        }
        return masks;
    }();
    
    // Adjacent files ONLY (without center file) - optimization for isolated pawn check
    constexpr std::array<uint64_t, 8> ADJACENT_FILES_ONLY = []() constexpr {
        std::array<uint64_t, 8> masks{};
        for (int f = 0; f < 8; ++f) {
            uint64_t m = 0;
            if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
            if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
            masks[f] = m;
        }
        return masks;
    }();
    
    // Precalculated adjacent files mask (including center file)
    constexpr std::array<uint64_t, 8> ADJACENT_AND_FILE_MASKS = []() constexpr {
        std::array<uint64_t, 8> masks{};
        for (int f = 0; f < 8; ++f) {
            uint64_t m = (0x0101010101010101ULL << f); // center file
            if (f > 0) m |= (0x0101010101010101ULL << (f - 1)); // left
            if (f < 7) m |= (0x0101010101010101ULL << (f + 1)); // right
            masks[f] = m;
        }
        return masks;
    }();
    
    // Dark/Light square masks for bad bishop evaluation
    constexpr uint64_t DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
    constexpr uint64_t LIGHT_SQUARES = ~DARK_SQUARES;
    
    // King proximity masks (squares at distance <= 2 from each square)
    constexpr std::array<uint64_t, 64> KING_PROXIMITY_MASKS = []() constexpr {
        std::array<uint64_t, 64> masks{};
        for (int sq = 0; sq < 64; ++sq) {
            uint64_t mask = 0;
            const int rank = chess::Board::rankOf(sq);
            const int file = chess::Board::fileOf(sq);
            
            // All squares within Manhattan distance 2
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
}

__attribute__((hot))
int64_t Engine::evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
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
        
        // Passed pawn check (no enemy pawns in front on same/adjacent files)
        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = WHITE_FORWARD_FILL[sq];
        if ((blackPawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score += PASSED_PAWN_BONUS;
            if (isEndgame) {
                score += (rank - 1) * 6; // Closer to promotion = better
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
        
        // Passed pawn check
        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = BLACK_FORWARD_FILL[sq];
        if ((whitePawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score -= PASSED_PAWN_BONUS;
            if (isEndgame) {
                score -= (7 - rank - 1) * 6; // Closer to promotion = better
            }
        }
    }
    
    return score;
}

int64_t Engine::evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;
    
    // WHITE
    if ((b.pawns_bb[0] & (chess::Board::bitMask(27))) && (occ & (chess::Board::bitMask(35)))) {
        if (b.knights_bb[0] & ((chess::Board::bitMask(18)) | (chess::Board::bitMask(21)))) score -= 10;
        if (b.bishops_bb[0] & ((chess::Board::bitMask(19)) | (chess::Board::bitMask(20)))) score -= 10;
        score -= 15;
    }

    // BLACK
    if ((b.pawns_bb[1] & (chess::Board::bitMask(35))) && (occ & (chess::Board::bitMask(27)))) {
        if (b.knights_bb[1] & ((chess::Board::bitMask(42)) | (chess::Board::bitMask(45)))) score += 10;
        if (b.bishops_bb[1] & ((chess::Board::bitMask(43)) | (chess::Board::bitMask(44)))) score += 10;
        score += 15;
    }

    return score;
}


int64_t Engine::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    int64_t score = 0;

    // Evaluate white rooks
    uint64_t rooks = whiteRooks;
    while (rooks) {
        const int sq = popLSB(rooks);
        const int file = sq & 7;
        const int rank = sq >> 3;
        const uint64_t fm = FILE_MASKS[file];

        const bool ownPawnOnFile = (whitePawns & fm) != 0;
        const bool oppPawnOnFile = (blackPawns & fm) != 0;

        // Branchless: bonus for open/semi-open file
        const int64_t fileBonus = (!ownPawnOnFile) * ((!oppPawnOnFile) ? OPEN_FILE_ROOK_BONUS : SEMI_OPEN_FILE_ROOK_BONUS);
        score += fileBonus;

        score += (rank == 6) * ROOK_ON_SEVENTH_BONUS;
    }

    // Evaluate black rooks
    rooks = blackRooks;
    while (rooks) {
        const int sq = popLSB(rooks);
        const int file = sq & 7;
        const int rank = sq >> 3;
        const uint64_t fm = FILE_MASKS[file];

        const bool ownPawnOnFile = (blackPawns & fm) != 0;
        const bool oppPawnOnFile = (whitePawns & fm) != 0;

        // Branchless: bonus for open/semi-open file (negative for black)
        const int64_t fileBonus = (!ownPawnOnFile) * ((!oppPawnOnFile) ? -OPEN_FILE_ROOK_BONUS : -SEMI_OPEN_FILE_ROOK_BONUS);
        score += fileBonus;

        score += (rank == 1) * (-ROOK_ON_SEVENTH_BONUS);
    }

    return score;
}

__attribute__((hot))
int64_t Engine::evalPassiveRooks(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        uint64_t rooks = b.rooks_bb[side];
        const uint64_t ownPawns = b.pawns_bb[side];

        while (rooks) {
            const int sq = popLSB(rooks);
            const int file = sq & 7;
            const int rank = sq >> 3;

            // OPTIMIZATION: compute mobility only when necessary (early exit)
            const uint64_t attacks = pieces::getRookAttacks(sq, occ);
            const int mobility = __builtin_popcountll(attacks & ~occ);

            // Low mobility - check FIRST (most common case)
            if (mobility <= 3) [[unlikely]] {
                score -= sign * 25;
            }

            // Rook blocked by own pawn on the same file - use precomputed masks
            if (ownPawns & FILE_MASKS[file]) [[unlikely]] {
                score -= sign * 15;
            }

            // Not on 7th rank penalty
            if ((side == 0 && rank != 6) || (side == 1 && rank != 1)) {
                score -= sign * 10;
            }
        }
    }

    return score;
}


__attribute__((hot))
int64_t Engine::evalKnightOnRim(const chess::Board& b) noexcept {
    // OPTIMIZATION: use precomputed masks instead of loops with if
    constexpr uint64_t A_FILE = 0x0101010101010101ULL;
    constexpr uint64_t H_FILE = 0x8080808080808080ULL;
    constexpr uint64_t B_FILE = 0x0202020202020202ULL;
    constexpr uint64_t G_FILE = 0x4040404040404040ULL;
    constexpr uint64_t RANK_1 = 0xFF00000000000000ULL;
    constexpr uint64_t RANK_8 = 0x00000000000000FFULL;
    
    constexpr uint64_t RIM_FILES = A_FILE | H_FILE;
    constexpr uint64_t NEAR_RIM_FILES = B_FILE | G_FILE;
    constexpr uint64_t BACK_RANKS = RANK_1 | RANK_8;
    
    int64_t score = 0;

    // WHITE knights (penalty per white = score negativo)
    const int whiteOnRim = __builtin_popcountll(b.knights_bb[0] & RIM_FILES);
    const int whiteNearRim = __builtin_popcountll(b.knights_bb[0] & NEAR_RIM_FILES);
    const int whiteBackRank = __builtin_popcountll(b.knights_bb[0] & BACK_RANKS);
    score -= whiteOnRim * 30 + whiteNearRim * 15 + whiteBackRank * 10;

    // BLACK knights (penalty per black = score positivo)
    const int blackOnRim = __builtin_popcountll(b.knights_bb[1] & RIM_FILES);
    const int blackNearRim = __builtin_popcountll(b.knights_bb[1] & NEAR_RIM_FILES);
    const int blackBackRank = __builtin_popcountll(b.knights_bb[1] & BACK_RANKS);
    score -= -(blackOnRim * 30 + blackNearRim * 15 + blackBackRank * 10);

    return score;
}


__attribute__((hot))
int64_t Engine::evalPieceCoordination(const chess::Board& b) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;

        // Minor pieces: knights and bishops
        uint64_t minors = b.knights_bb[side] | b.bishops_bb[side];
        if (!minors) continue;

        // All friendly pieces (including pawns) - used to check proximity
        const uint64_t friends = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side];

        while (minors) {
            const int sq = popLSB(minors);
            const uint64_t nearby = KING_PROXIMITY_MASKS[sq];
            if ((friends & nearby) == 0) {
                // No friendly piece within Manhattan distance <= 2
                score -= sign * COORDINATION_PENALTY;
            }
        }
    }

    return score;
}


__attribute__((hot))
int64_t Engine::evalOutposts(const chess::Board& b) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;

        // Consider knights and bishops for outposts (knights benefit more)
        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = popLSB(knights);
            const int opp = side ^ 1;

            // Outpost: supported by a friendly pawn and not attacked by enemy pawns
            const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[side][sq] & b.pawns_bb[side]) != 0;
            const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[opp][sq] & b.pawns_bb[opp]) != 0;

            if (supportedByPawn && !attackedByEnemyPawn) {
                score += sign * OUTPOST_KNIGHT_BONUS; // knight outpost bonus
            }
        }

        uint64_t bishops = b.bishops_bb[side];
        while (bishops) {
            const int sq = popLSB(bishops);
            const int opp = side ^ 1;
            const bool supportedByPawn = (pieces::PAWN_ATTACKERS_TO[side][sq] & b.pawns_bb[side]) != 0;
            const bool attackedByEnemyPawn = (pieces::PAWN_ATTACKERS_TO[opp][sq] & b.pawns_bb[opp]) != 0;
            if (supportedByPawn && !attackedByEnemyPawn) {
                score += sign * (OUTPOST_BISHOP_BONUS / 2); // smaller bonus for bishops
            }
        }
    }

    return score;
}


__attribute__((hot, always_inline))
inline int64_t Engine::evalMobility(const AttackData data[2]) noexcept {
    return (data[0].knightMobility + data[0].bishopMobility + data[0].rookMobility + data[0].queenMobility
          - data[1].knightMobility - data[1].bishopMobility - data[1].rookMobility - data[1].queenMobility) / 2;
}

__attribute__((hot, always_inline))
inline int64_t Engine::evalInitiative(const chess::Board& b, bool isEndgame) noexcept {
    const int64_t bonus = isEndgame ? INIT_BONUS_EG : INIT_BONUS_MG;
    return (b.getActiveColor() == chess::Board::WHITE) ? bonus : -bonus;
}

__attribute__((hot))
int64_t Engine::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    // OPTIMIZATION: calculate in batches instead of per-bishop loops
    constexpr uint64_t DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
    
    const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
    const int lightPawnCount = __builtin_popcountll(pawns & ~DARK_SQUARES);
    
    // Conta alfieri su caselle scure/chiare
    const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
    const int lightBishops = __builtin_popcountll(bishops & ~DARK_SQUARES);
    
    // Score = - (dark_bishops * dark_pawns + light_bishops * light_pawns) * 8
    const int64_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * 8);
    
    return (side == 0) ? score : -score;
}

// OPTIMIZATION: reward development using bitboard batches (no loop)
__attribute__((hot))
int64_t Engine::evalMinorPieceDevelopment(const chess::Board& b) noexcept {
    // Caselle iniziali per i pezzi minori
    // CAUTION: bit 0-7 = rank 8 (BLACK), bit 56-63 = rank 1 (WHITE)
    constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL; // rank 1 & 2 (bit 56-63 + 48-55)
    constexpr uint64_t BLACK_MINOR_START = 0x000000000000FFFFULL; // rank 8 & 7 (bit 0-7 + 8-15)
    
    // Conta pezzi sviluppati (fuori dalle caselle iniziali) in una sola operazione
    const int whiteDeveloped = 
        __builtin_popcountll(b.knights_bb[0] & ~WHITE_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[0] & ~WHITE_MINOR_START);
    
    const int blackDeveloped = 
        __builtin_popcountll(b.knights_bb[1] & ~BLACK_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[1] & ~BLACK_MINOR_START);
    
    return (whiteDeveloped - blackDeveloped) * DEVELOPMENT_BONUS;
}

int64_t Engine::evalEarlyKing(const chess::Board& b) noexcept {
    int64_t score = 0;

    if (b.kings_bb[0] && !(b.kings_bb[0] & chess::Board::bitMask(60)) && !(b.kings_bb[0] & chess::Board::bitMask(62))) {
        score += EARLY_KING_PENALTY; // già negativo
    }

    if (b.kings_bb[1] && !(b.kings_bb[1] & chess::Board::bitMask(4)) && !(b.kings_bb[1] & chess::Board::bitMask(6))) {
        score -= EARLY_KING_PENALTY; // già negativo
    }

    return score;
}

int64_t Engine::evalEarlyRook(const chess::Board& b) noexcept {
    int64_t score = 0;

    // White rooks
    if (b.rooks_bb[0] && !(b.rooks_bb[0] & chess::Board::bitMask(56)) && !(b.rooks_bb[0] & chess::Board::bitMask(63))) {
        score += EARLY_ROOK_PENALTY; // già negativo
    }

    // Black rooks
    if (b.rooks_bb[1] && !(b.rooks_bb[1] & chess::Board::bitMask(0)) && !(b.rooks_bb[1] & chess::Board::bitMask(7))) {
        score -= EARLY_ROOK_PENALTY;
    }

    return score;
}

int64_t Engine::evalEarlyQueen(const chess::Board& b) noexcept {
    int64_t score = 0;

    // White queen
    if (b.queens_bb[0] && !(b.queens_bb[0] & chess::Board::bitMask(59))) {
        score += ATTACKED_QUEEN_PENALTY * 8; // già negativo
    }

    // Black queen
    if (b.queens_bb[1] && !(b.queens_bb[1] & chess::Board::bitMask(3))) {
        score -= ATTACKED_QUEEN_PENALTY * 8;
    }

    return score;
}

int64_t Engine::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    // NOTE: This function needs per-piece mobility, not aggregate mobility from AttackData
    // We still need to iterate through individual pieces to check if each one is trapped
    int64_t score = 0;

    // Small extra penalty to make truly trapped pieces slightly worse than the
    // base PINNED_* penalties. This keeps tuning local to the evaluation
    // function and avoids changing global constants.
    constexpr int64_t TRAPPED_EXTRA_SEVERITY = 10; // in centipawns

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;

        // Knights: use a precomputed lookup table (no magic bitboards)
        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = popLSB(knights);
            const int mobility = __builtin_popcountll((pieces::KNIGHT_ATTACKS[sq]) & ~occ);
            if (mobility == 0) [[unlikely]] score -= sign * (PINNED_KNIGHT_PENALTY + TRAPPED_EXTRA_SEVERITY);
            else if (mobility <= 3) score -= sign * LOW_MOBILITY_KNIGHT_PENALTY;
        }

        // Bishops, Rooks, Queens: calcola solo se pochi pezzi (risparmia magic bitboard lookups)
        const int pieceCount = __builtin_popcountll(b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]);
        
        if (pieceCount > 0) [[likely]] {
            uint64_t bishops = b.bishops_bb[side];
            while (bishops) {
                const int sq = popLSB(bishops);
                const int mobility = __builtin_popcountll(pieces::getBishopAttacks(sq, occ) & ~occ);
                if (mobility == 0) [[unlikely]] score -= sign * (PINNED_BISHOP_PENALTY + TRAPPED_EXTRA_SEVERITY);
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_BISHOP_PENALTY;
            }

            uint64_t rooks = b.rooks_bb[side];
            while (rooks) {
                const int sq = popLSB(rooks);
                const int mobility = __builtin_popcountll(pieces::getRookAttacks(sq, occ) & ~occ);
                if (mobility == 0) [[unlikely]] score -= sign * (PINNED_ROOK_PENALTY + TRAPPED_EXTRA_SEVERITY);
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_ROOK_PENALTY;
            }

            uint64_t queens = b.queens_bb[side];
            while (queens) {
                const int sq = popLSB(queens);
                const int mobility = __builtin_popcountll(pieces::getQueenAttacks(sq, occ) & ~occ);
                if (mobility == 0) [[unlikely]] score -= sign * (PINNED_QUEEN_PENALTY + TRAPPED_EXTRA_SEVERITY);
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_QUEEN_PENALTY;
            }
        }
    }

    return score;
}

int64_t Engine::evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        const int opp  = side ^ 1;

        // Use precomputed attack maps!
        uint64_t enemyAttacks = data[opp].allAttacks;
        uint64_t friendlyDef = data[side].allAttacks;

        // Hanging pieces (attacked but undefended)
        // IMPORTANTE: i penalty sono già negativi, quindi usiamo += con sign
        uint64_t hanging;

        hanging = b.pawns_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_PAWN_PENALTY;

        hanging = b.knights_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_MINOR_PENALTY;

        hanging = b.bishops_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_MINOR_PENALTY;

        hanging = b.rooks_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_ROOK_PENALTY;

        hanging = b.queens_bb[side] & enemyAttacks & ~friendlyDef;
        score += sign * __builtin_popcountll(hanging) * HANGING_QUEEN_PENALTY;
    }

    return score;
}

__attribute__((hot, always_inline))
inline int64_t Engine::evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint64_t CENTER_MASK = 0x0000001818000000ULL; // e4,d4,e5,d5
    return (__builtin_popcountll(whitePawns & CENTER_MASK) - __builtin_popcountll(blackPawns & CENTER_MASK)) * CENTER_CONTROL_BONUS;
}

int64_t Engine::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    // Cheap pawn-shield evaluation. Avoid undefined shifts around edges.
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const uint64_t kingBB = b.kings_bb[side];
        if (!kingBB) [[unlikely]] continue;
        const int sq = __builtin_ctzll(kingBB);
        const int sign = (side == 0) ? 1 : -1;

        // Check if castled
        const bool hasCastled = (side == 0 && (sq == 62 || sq == 58)) || (side == 1 && (sq == 6 || sq == 2));
        
        if (!hasCastled) {
            score -= sign * 20; // penalità fissa se non arroccato
            
            // OPTIMIZATION: Penalize moving kingside/queenside pawns before castling
            // This weakens king safety if castling rights are still available
            const bool canCastleKingside = (side == 0) ? b.getCastle(0) : b.getCastle(2);
            const bool canCastleQueenside = (side == 0) ? b.getCastle(1) : b.getCastle(3);
            
            if (side == 0) { // WHITE
                // Kingside castling pawns: f2(53), g2(54), h2(55)
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(53) | chess::Board::bitMask(54) | chess::Board::bitMask(55);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~whitePawns);
                    score -= movedPawns * 5; // penalità ridotta: 5cp per pedone
                }
                
                // Queenside castling pawns: b2(49), c2(50), d2(51)
                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(49) | chess::Board::bitMask(50) | chess::Board::bitMask(51);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~whitePawns);
                    score -= movedPawns * 5; // penalità ridotta: 5cp per pedone
                }
            } else { // BLACK
                // Kingside castling pawns: f7(13), g7(14), h7(15)
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(13) | chess::Board::bitMask(14) | chess::Board::bitMask(15);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~blackPawns);
                    score += movedPawns * 5; // bonus per WHITE (black indebolito)
                }
                
                // Queenside castling pawns: b7(9), c7(10), d7(11)
                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(9) | chess::Board::bitMask(10) | chess::Board::bitMask(11);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~blackPawns);
                    score += movedPawns * 5; // bonus per WHITE (black indebolito)
                }
            }
        }
        
        uint64_t shieldSquares = 0ULL;
        if (side == 0) {
            // White king: pawns in front are towards lower indices (south)
            if (sq >= 8) shieldSquares |= chess::Board::bitMask(sq - 8);
            if (sq >= 7 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq - 7);
            if (sq >= 9 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq - 9);
            score += sign * __builtin_popcountll(whitePawns & shieldSquares) * 10;
        } else {
            // Black king: pawns in front are towards higher indices (north)
            if (sq <= 55) shieldSquares |= chess::Board::bitMask(sq + 8);
            if (sq <= 56 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq + 7);
            if (sq <= 54 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq + 9);
            score += sign * __builtin_popcountll(blackPawns & shieldSquares) * 10;
        }
    }

    return score;
}

int Engine::manhattan(int a, int b) noexcept {
    return std::abs((a & 7) - (b & 7)) + std::abs((a >> 3) - (b >> 3));
}

__attribute__((hot))
int64_t Engine::evalKingActivity(const chess::Board& b, bool isEndgame) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        const uint64_t kingBB = b.kings_bb[side];
        if (!kingBB) [[unlikely]] continue;

        const int ksq = __builtin_ctzll(kingBB);

        // OPTIMIZATION: use precomputed proximity masks (distance <= 2)
        // This replaces the expensive manhattan distance loops
        const uint64_t proximityMask = KING_PROXIMITY_MASKS[ksq];

        // ENDGAME: king activity (count nearby friendly pieces)
        if (isEndgame) {
            const uint64_t friends =
                b.pawns_bb[side]   |
                b.knights_bb[side] |
                b.bishops_bb[side] |
                b.rooks_bb[side]   |
                b.queens_bb[side];
            
            // Single popcount instead of loop with manhattan!
            const int friendsNearKing = __builtin_popcountll(friends & proximityMask);
            score += sign * friendsNearKing * KING_ACTIVITY_BONUS;
        }

        // MIDGAME ONLY: enemy proximity penalty (count nearby enemy pieces)
        // In endgame, the king needs to attack, so we don't penalize enemy proximity
        if (!isEndgame) {
            const uint64_t enemies =
                b.pawns_bb[side ^ 1]   |
                b.knights_bb[side ^ 1] |
                b.bishops_bb[side ^ 1] |
                b.rooks_bb[side ^ 1]   |
                b.queens_bb[side ^ 1];

            // Single popcount instead of loop with manhattan!
            const int enemiesNearKing = __builtin_popcountll(enemies & proximityMask);
            score += sign * enemiesNearKing * KING_SAFETY_PENALTY;
        }
    }

    return score;
}

int64_t Engine::evalBadKingPosition(const chess::Board& b) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        const uint64_t kingBB = b.kings_bb[side];
        if (!kingBB) [[unlikely]] continue;

        const int sq = __builtin_ctzll(kingBB);
        const int rank = sq >> 3;

        // White re sopra 2a traversa o Black sotto 7a
        if ((side == 0 && rank < 6) || (side == 1 && rank > 1)) {
            score += sign * KING_EXPOSED_PENALTY;
        }
    }

    return score;
}


int64_t Engine::evalEndgameKingActivity(const chess::Board& b) noexcept {
    constexpr int CENTER[4] = {27, 28, 35, 36}; // d4 e4 d5 e5
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const uint64_t kbb = b.kings_bb[side];
        if (!kbb) [[unlikely]] continue;

        const int sq = __builtin_ctzll(kbb);
        int best = 99;
        for (int c : CENTER)
            best = std::min(best, manhattan(sq, c));

        score += (side == 0 ? -best : best) * 10;
    }
    return score;
}

int64_t Engine::evalCastlingBonus(const chess::Board& b) noexcept {
    // Castling positions (a8=0, h1=63):
    // White: g1=62 (kingside), c1=58 (queenside), f1=61, d1=59
    // Black: g8=6 (kingside), c8=2 (queenside), f8=5, d8=3
    constexpr uint64_t WHITE_KING_CASTLED  = (chess::Board::bitMask(62) | chess::Board::bitMask(58));
    constexpr uint64_t WHITE_ROOK_CASTLED  = (chess::Board::bitMask(61) | chess::Board::bitMask(59));
    constexpr uint64_t BLACK_KING_CASTLED  = (chess::Board::bitMask(6)  | chess::Board::bitMask(2));
    constexpr uint64_t BLACK_ROOK_CASTLED  = (chess::Board::bitMask(5)  | chess::Board::bitMask(3));

    int64_t score = 0;

    // WHITE
    bool whiteHasCastled = (b.kings_bb[0] & WHITE_KING_CASTLED) && (b.rooks_bb[0] & WHITE_ROOK_CASTLED);
    bool whiteCanCastle = b.getCastle(0) || b.getCastle(1); // kingside or queenside
    
    if (whiteHasCastled) {
        score += CASTLING_BONUS;
    } else if (!whiteCanCastle) {
        // Ha perso il diritto di arroccare senza aver arroccato! PENALITA' GRAVE
        score -= 60;
    }

    // BLACK
    bool blackHasCastled = (b.kings_bb[1] & BLACK_KING_CASTLED) && (b.rooks_bb[1] & BLACK_ROOK_CASTLED);
    bool blackCanCastle = b.getCastle(2) || b.getCastle(3); // kingside or queenside
    
    if (blackHasCastled) {
        score -= CASTLING_BONUS;
    } else if (!blackCanCastle) {
        // Ha perso il diritto di arroccare senza aver arroccato! PENALITA' GRAVE
        score += 60;
    }

    return score;
}

__attribute__((hot))
void Engine::computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
    // OPTIMIZATION: initialize to zero with memset (faster)
    std::memset(data, 0, 2 * sizeof(AttackData));

    // OPTIMIZATION: process both sides to improve cache locality
    for (int side = 0; side < 2; ++side) {
        AttackData& d = data[side];
        
        // Pawns - usa lookup table (no magic bitboards)
        uint64_t pawns = b.pawns_bb[side];
        while (pawns) {
            const int sq = popLSB(pawns);
            d.pawnAttacks |= pieces::PAWN_ATTACKS[side][sq];
        }
        d.allAttacks = d.pawnAttacks;

        // Knights - usa lookup table (no magic bitboards)
        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = popLSB(knights);
            const uint64_t attacks = pieces::KNIGHT_ATTACKS[sq];
            d.knightAttacks |= attacks;
            d.knightMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.knightAttacks;

        // Bishops - magic bitboards necessari
        uint64_t bishops = b.bishops_bb[side];
        while (bishops) {
            const int sq = popLSB(bishops);
            const uint64_t attacks = pieces::getBishopAttacks(sq, occ);
            d.bishopAttacks |= attacks;
            d.bishopMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.bishopAttacks;

        // Rooks - magic bitboards necessari
        uint64_t rooks = b.rooks_bb[side];
        while (rooks) {
            const int sq = popLSB(rooks);
            const uint64_t attacks = pieces::getRookAttacks(sq, occ);
            d.rookAttacks |= attacks;
            d.rookMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.rookAttacks;

        // Queens - magic bitboards necessari
        uint64_t queens = b.queens_bb[side];
        while (queens) {
            const int sq = popLSB(queens);
            const uint64_t attacks = pieces::getQueenAttacks(sq, occ);
            d.queenAttacks |= attacks;
            d.queenMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.queenAttacks;
        
        // Mark as computed
        d.isComputed = true;
    }
}


int64_t Engine::evalBlockedPawnByBishops(const chess::Board& b) noexcept {
    int64_t score = 0;

    // For each pawn, check if a friendly bishop sits on the square directly in front
    // (i.e., blocks pawn advance). Penalize more heavily for central files (d/e)
    // and if the pawn is still on its initial rank (d2/e2 or d7/e7).
    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        uint64_t pawns = b.pawns_bb[side];
        const uint64_t bishops = b.bishops_bb[side]; // own bishops

        if (!pawns || !bishops) continue;

        while (pawns) {
            const int psq = popLSB(pawns);
            const int rank = chess::Board::rankOf(psq);
            const int file = chess::Board::fileOf(psq);

            const int forward = (side == 0) ? (psq - 8) : (psq + 8);
            if (forward < 0 || forward >= 64) continue;

            if (bishops & chess::Board::bitMask(forward)) {
                // base penalty
                int penalty = 40; // base
                // central files (d=3, e=4) are worse
                if (file == 3 || file == 4) penalty += 45;
                // extra penalty if pawn still on starting rank (white:6, black:1)
                const int startRank = (side == 0) ? 6 : 1;
                if (rank == startRank) penalty += 30;

                score -= sign * penalty;
            }
        }
    }

    return score;
}





__attribute__((hot))
int64_t Engine::evaluate(const chess::Board& board) noexcept {
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0 || board.isCheckmate(board.getActiveColor())) [[unlikely]] {
        //(missing king)
        return evaluateCheckmate(board);
    }

    int64_t eval = getMaterialDelta(board);

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
    constexpr int PIECE_ENDGAME_THRESHOLD = 8;  // <= 8 pezzi = endgame
    
    const bool isOpening = (fullMoves < OPENING_MOVES);
    const bool isEarlyMiddlegame = (fullMoves >= OPENING_MOVES && fullMoves < EARLY_MG_MOVES);
    const bool isEndgame = (nonPawnMajors <= PIECE_ENDGAME_THRESHOLD);
    const bool isMiddlegame = !isOpening && !isEndgame;

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
        eval += evalEarlyKing(board);
        eval += evalEarlyRook(board);
        eval += evalEarlyQueen(board);
        
        // Castling is CRITICAL in opening
        eval += evalCastlingBonus(board);
        
        // Basic piece safety (avoid hanging pieces) - NEEDS attackData
        
        eval += evalHangingPieces(board, attackData);
        
        // Center control è FONDAMENTALE in opening
        eval += evalCentralControl(whitePawns, blackPawns);
        
        // Knight positioning (avoid rim)
        eval += evalKnightOnRim(board);
        // Penalize non-coordinated minor pieces (promote piece coordination)
        eval += evalPieceCoordination(board);
        // Outposts: reward stable knights/bishops supported by pawns and not attacked by enemy pawns
        eval += evalOutposts(board);
        
        // Basic pawn structure (non troppo dettagliato)
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        
        // Mobility bonus (sviluppare pezzi = più mosse) - NEEDS attackData
        eval += evalMobility(attackData);
        
        // Initiative bonus (side to move advantage)
        eval += evalInitiative(board, false);
    // Penalize bishops that block pawns directly (opening)
    eval += evalBlockedPawnByBishops(board);
    }
    
    // ===================================================
    // EARLY MIDDLEGAME (moves 10-15)
    // Transition phase: continue development, prepare attacks
    // ===================================================
    else if (isEarlyMiddlegame) {
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
        eval += evalKnightOnRim(board);
        eval += evalPieceCoordination(board);
        eval += evalOutposts(board);
        eval += evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        
        // King safety starts to matter
        eval += evalKingSafety(board, whitePawns, blackPawns);
        eval += evalBadKingPosition(board);
        
        // Rook evaluation
        eval += evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        
        // Initiative
        eval += evalInitiative(board, false);
    // Penalize bishops that block pawns in early middlegame
    eval += evalBlockedPawnByBishops(board);
    }
    
    // ===================================================
    // MIDDLEGAME (moves 15+, many pieces on board)
    // Focus: tactics, king safety, piece coordination
    // ===================================================
    else if (isMiddlegame) {
        // Full tactical evaluation (molto importante!) - NEEDS attackData
        eval += evalHangingPieces(board, attackData);
        eval += evalTrappedPieces(board, occ);
        
        // Pawn structure evaluation
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        eval += evalCentralControl(whitePawns, blackPawns);
        eval += evalBlockedCenterWithPieces(board, occ);
        
        // Piece activity and positioning - NEEDS attackData
        eval += evalMobility(attackData);
        eval += evalKnightOnRim(board);
        eval += evalPieceCoordination(board);
        eval += evalOutposts(board);
        eval += evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        
        // King safety è CRITICO in middlegame
        eval += evalKingSafety(board, whitePawns, blackPawns);
        eval += evalBadKingPosition(board);
        eval += evalKingActivity(board, false);
        eval += evalCastlingBonus(board);
        
        // Rook evaluation (open files, 7th rank)
        eval += evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        eval += evalPassiveRooks(board, occ);
        
        // Initiative
        eval += evalInitiative(board, false);
    // Penalize bishops that block pawns in middlegame
    eval += evalBlockedPawnByBishops(board);
    }
    
    // ===================================================
    // ENDGAME (few pieces left)
    // Focus: pawn promotion, king activity, passed pawns
    // ===================================================
    else { // isEndgame
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
        
        // Minor piece positioning
        eval += evalKnightOnRim(board);
        eval += evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        
        // Initiative (meno importante in endgame)
        eval += evalInitiative(board, true);
    }

    return eval;
}

/*
bool Engine::isMate() noexcept {
    uint8_t toMove = board.getActiveColor();
    return board.kings_bb[0] == 0    || board.kings_bb[1] == 0 
        || board.isCheckmate(toMove) || board.isDraw(toMove);
}
*/

void Engine::updateGameResult() noexcept {
    gameResult = GameResult::ONGOING;
    uint8_t toMove = board.getActiveColor();
    if (board.kings_bb[0] == 0) {
        gameResult = GameResult::BLACK_WINS;
    } else if (board.kings_bb[1] == 0) {
        gameResult = GameResult::WHITE_WINS;
    } else if (board.isCheckmate(toMove)) {
        gameResult = (toMove == chess::Board::WHITE) ? GameResult::BLACK_WINS : GameResult::WHITE_WINS;
    } else if (board.isDraw(toMove)) {
        gameResult = GameResult::DRAW;
    }
}

}; // namespace engine
