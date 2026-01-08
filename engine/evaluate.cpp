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
        uint8_t sq = __builtin_ctzll(bbWhite);
        bbWhite &= (bbWhite - 1);
        eval += table[sq];
    }
    // Black pieces: mirror index vertically (inline for performance)
    while (bbBlack) {
        uint8_t sq = __builtin_ctzll(bbBlack);
        bbBlack &= (bbBlack - 1);
        eval -= table[sq ^ 56]; // Branchless vertical mirror (flip rank)
    }
}

__attribute__((always_inline))
inline int poplsbIndex(uint64_t& bb) noexcept {
    const int sq = __builtin_ctzll(bb);
    bb &= (bb - 1);
    return sq;
}

__attribute__((always_inline))
inline uint64_t fileMask(int file) noexcept {
    // Table-driven: avoids a variable shift on the hot path.
    static constexpr uint64_t FILE_MASKS[8] = {
        0x0101010101010101ULL,
        0x0202020202020202ULL,
        0x0404040404040404ULL,
        0x0808080808080808ULL,
        0x1010101010101010ULL,
        0x2020202020202020ULL,
        0x4040404040404040ULL,
        0x8080808080808080ULL,
    };
    return FILE_MASKS[file];
}

__attribute__((always_inline))
inline uint64_t adjacentFilesMask(int file) noexcept {
    uint64_t m = 0;
    if (file > 0) m |= fileMask(file - 1);
    if (file < 7) m |= fileMask(file + 1);
    return m;
}

// Precompute masks for faster pawn evaluation
namespace {
    // Forward fill masks for passed pawn detection (compile-time constant)
    constexpr std::array<uint64_t, 64> initWhiteForwardFill() {
        std::array<uint64_t, 64> result{};
        for (int sq = 0; sq < 64; ++sq) {
            const int rank = sq >> 3;
            result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
        }
        return result;
    }
    
    constexpr std::array<uint64_t, 64> initBlackForwardFill() {
        std::array<uint64_t, 64> result{};
        for (int sq = 0; sq < 64; ++sq) {
            const int rank = sq >> 3;
            result[sq] = (rank > 0) ? ((1ULL << (rank * 8)) - 1ULL) : 0ULL;
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
        const int sq = poplsbIndex(wp);
        const int file = sq & 7;
        const int rank = sq >> 3;
        
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
        const int sq = poplsbIndex(bp);
        const int file = sq & 7;
        const int rank = sq >> 3;
        
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

/*
int64_t Engine::evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    int64_t score = 0;

    // Doubled pawns: count per-file pawn excess over 1.
    for (int f = 0; f < 8; ++f) {
        const uint64_t fm = fileMask(f);
        const int wCount = __builtin_popcountll(whitePawns & fm);
        const int bCount = __builtin_popcountll(blackPawns & fm);
        if (wCount > 1){ score += (wCount - 1) * DOUBLED_PAWN_PENALTY; }
        if (bCount > 1){ score -= (bCount - 1) * DOUBLED_PAWN_PENALTY; }
    }

    // Isolated pawns: no friendly pawns on adjacent files.
    uint64_t wp = whitePawns;
    while (wp) {
        const int sq = poplsbIndex(wp);
        const int file = sq & 7;
        if ((whitePawns & adjacentFilesMask(file)) == 0) score += ISOLATED_PAWN_PENALTY;
    }
    uint64_t bp = blackPawns;
    while (bp) {
        const int sq = poplsbIndex(bp);
        const int file = sq & 7;
        if ((blackPawns & adjacentFilesMask(file)) == 0) score -= ISOLATED_PAWN_PENALTY;
    }

    // Passed pawns: no enemy pawn in same/adjacent file ahead.
    // This is simplified but correct and safe.
    wp = whitePawns;
    while (wp) {
        const int sq = poplsbIndex(wp);
        const int file = sq & 7;
        const uint64_t lanes = fileMask(file) | adjacentFilesMask(file);
        // Squares in front of sq for white are ranks > current rank.
        const int rank = sq >> 3;
        const uint64_t inFront = 0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8);
        if ((blackPawns & lanes & inFront) == 0) {
            score += PASSED_PAWN_BONUS;
            // Endgame scaling: bonus increases as pawn advances (lower rank = closer to promotion)
            if (isEndgame) {
                score += (rank - 1) * 6;
            }
        }
    }
    bp = blackPawns;
    while (bp) {
        const int sq = poplsbIndex(bp);
        const int file = sq & 7;
        const uint64_t lanes = fileMask(file) | adjacentFilesMask(file);
        // Squares in front of sq for black are ranks < current rank.
        const int rank = sq >> 3;
        const uint64_t inFront = (rank == 0) ? 0ULL : ((1ULL << (rank * 8)) - 1ULL);
        if ((whitePawns & lanes & inFront) == 0) {
            score -= PASSED_PAWN_BONUS;
            // Endgame scaling: bonus increases as pawn advances (higher rank = closer to promotion)
            if (isEndgame) {
                score -= (7 - rank - 1) * 6;
            }
        }
    }

    return score;
}
*/
int64_t Engine::evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;
    
    // WHITE
    if ((b.pawns_bb[0] & (1ULL << 27)) && (occ & (1ULL << 35))) {
        if (b.knights_bb[0] & ((1ULL << 18) | (1ULL << 21))) score -= 10;
        if (b.bishops_bb[0] & ((1ULL << 19) | (1ULL << 20))) score -= 10;
        score -= 15;
    }

    // BLACK
    if ((b.pawns_bb[1] & (1ULL << 35)) && (occ & (1ULL << 27))) {
        if (b.knights_bb[1] & ((1ULL << 42) | (1ULL << 45))) score += 10;
        if (b.bishops_bb[1] & ((1ULL << 43) | (1ULL << 44))) score += 10;
        score += 15;
    }

    return score;
}


int64_t Engine::evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    int64_t score = 0;

    auto evalSide = [&](uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns, int sign) {
        while (rooks) {
            const int sq = poplsbIndex(rooks);
            const int file = sq & 7;
            const int rank = sq >> 3;
            const uint64_t fm = FILE_MASKS[file]; // Use precalculated mask

            const bool ownPawnOnFile = (ownPawns & fm);
            const bool oppPawnOnFile = (oppPawns & fm);

            if (!ownPawnOnFile && !oppPawnOnFile)
                score += sign * OPEN_FILE_ROOK_BONUS;
            else if (!ownPawnOnFile && oppPawnOnFile)
                score += sign * SEMI_OPEN_FILE_ROOK_BONUS;

            // 7th rank (white = rank 6, black = rank 1)
            if ((sign == 1 && rank == 6) || (sign == -1 && rank == 1))
                score += sign * ROOK_ON_SEVENTH_BONUS;
        }
    };

    evalSide(whiteRooks, whitePawns, blackPawns, +1);
    evalSide(blackRooks, blackPawns, whitePawns, -1);

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
            const int sq = poplsbIndex(rooks);
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

    if (b.kings_bb[0] && !(b.kings_bb[0] & (1ULL << 60)) && !(b.kings_bb[0] & (1ULL << 62))) {
        score += EARLY_KING_PENALTY; // già negativo
    }

    if (b.kings_bb[1] && !(b.kings_bb[1] & (1ULL << 4)) && !(b.kings_bb[1] & (1ULL << 6))) {
        score -= EARLY_KING_PENALTY; // già negativo
    }

    return score;
}

int64_t Engine::evalEarlyRook(const chess::Board& b) noexcept {
    int64_t score = 0;

    // White rooks
    if (b.rooks_bb[0] && !(b.rooks_bb[0] & (1ULL << 56)) && !(b.rooks_bb[0] & (1ULL << 63))) {
        score += EARLY_ROOK_PENALTY; // già negativo
    }

    // Black rooks
    if (b.rooks_bb[1] && !(b.rooks_bb[1] & (1ULL << 0)) && !(b.rooks_bb[1] & (1ULL << 7))) {
        score -= EARLY_ROOK_PENALTY;
    }

    return score;
}

int64_t Engine::evalEarlyQueen(const chess::Board& b) noexcept {
    int64_t score = 0;

    // White queen
    if (b.queens_bb[0] && !(b.queens_bb[0] & (1ULL << 59))) {
        score += ATTACKED_QUEEN_PENALTY * 8; // già negativo
    }

    // Black queen
    if (b.queens_bb[1] && !(b.queens_bb[1] & (1ULL << 3))) {
        score -= ATTACKED_QUEEN_PENALTY * 8;
    }

    return score;
}

int64_t Engine::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    // NOTE: This function needs per-piece mobility, not aggregate mobility from AttackData
    // We still need to iterate through individual pieces to check if each one is trapped
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;

    // Knights: use a precomputed lookup table (no magic bitboards)
        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = poplsbIndex(knights);
            const int mobility = __builtin_popcountll((pieces::KNIGHT_ATTACKS[sq]) & ~occ);
            if (mobility == 0) [[unlikely]] score -= sign * PINNED_KNIGHT_PENALTY;
            else if (mobility <= 3) score -= sign * LOW_MOBILITY_KNIGHT_PENALTY;
        }

        // Bishops, Rooks, Queens: calcola solo se pochi pezzi (risparmia magic bitboard lookups)
        const int pieceCount = __builtin_popcountll(b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]);
        
        if (pieceCount > 0) [[likely]] {
            uint64_t bishops = b.bishops_bb[side];
            while (bishops) {
                const int sq = poplsbIndex(bishops);
                const int mobility = __builtin_popcountll(pieces::getBishopAttacks(sq, occ) & ~occ);
                if (mobility == 0) [[unlikely]] score -= sign * PINNED_BISHOP_PENALTY;
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_BISHOP_PENALTY;
            }

            uint64_t rooks = b.rooks_bb[side];
            while (rooks) {
                const int sq = poplsbIndex(rooks);
                const int mobility = __builtin_popcountll(pieces::getRookAttacks(sq, occ) & ~occ);
                if (mobility == 0) [[unlikely]] score -= sign * PINNED_ROOK_PENALTY;
                else if (mobility <= 3) score -= sign * LOW_MOBILITY_ROOK_PENALTY;
            }

            uint64_t queens = b.queens_bb[side];
            while (queens) {
                const int sq = poplsbIndex(queens);
                const int mobility = __builtin_popcountll(pieces::getQueenAttacks(sq, occ) & ~occ);
                if (mobility == 0) [[unlikely]] score -= sign * PINNED_QUEEN_PENALTY;
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

        // TODO: se non ha arroccato, NON muovere pedoni dell'arrocco
        if (!((side == 0 && (sq == 62 || sq == 58)) || (side == 1 && (sq == 6 || sq == 2)))) {
            score -= sign * 20; // penalità fissa se non arroccato
        }
        uint64_t shieldSquares = 0ULL;
        if (side == 0) {
            // White king: pawns in front are towards lower indices (south)
            if (sq >= 8) shieldSquares |= (1ULL << (sq - 8));
            if (sq >= 7 && (sq & 7) != 7) shieldSquares |= (1ULL << (sq - 7));
            if (sq >= 9 && (sq & 7) != 0) shieldSquares |= (1ULL << (sq - 9));
            score += sign * __builtin_popcountll(whitePawns & shieldSquares) * 10;
        } else {
            // Black king: pawns in front are towards higher indices (north)
            if (sq <= 55) shieldSquares |= (1ULL << (sq + 8));
            if (sq <= 56 && (sq & 7) != 0) shieldSquares |= (1ULL << (sq + 7));
            if (sq <= 54 && (sq & 7) != 7) shieldSquares |= (1ULL << (sq + 9));
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

        // OPTIMIZATION: use bitboards instead of loops to count nearby pieces
        // Precompute the king zone (adjacent squares) as a bitboard
        uint64_t kingZone = pieces::KING_ATTACKS[ksq]; // 8 caselle adiacenti

        // ENDGAME: king activity (conta alleati vicini con bitboard)
        if (isEndgame) {
            const uint64_t friends =
                b.pawns_bb[side]   |
                b.knights_bb[side] |
                b.bishops_bb[side] |
                b.rooks_bb[side]   |
                b.queens_bb[side];
            
            // Conta pezzi amici in king zone (molto più veloce del loop manhattan)
            const int friendsNearKing = __builtin_popcountll(friends & kingZone);
            score += sign * friendsNearKing * KING_ACTIVITY_BONUS;
        }

        // MIDGAME: enemy proximity penalty (conta nemici vicini con bitboard)
        const uint64_t enemies =
            b.pawns_bb[side ^ 1]   |
            b.knights_bb[side ^ 1] |
            b.bishops_bb[side ^ 1] |
            b.rooks_bb[side ^ 1]   |
            b.queens_bb[side ^ 1];

        // Conta pezzi nemici in king zone
        const int enemiesNearKing = __builtin_popcountll(enemies & kingZone);
        score += sign * enemiesNearKing * KING_SAFETY_PENALTY;
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
    constexpr uint64_t WHITE_KING_CASTLED  = (1ULL << 62) | (1ULL << 58);
    constexpr uint64_t WHITE_ROOK_CASTLED  = (1ULL << 61) | (1ULL << 59);
    constexpr uint64_t BLACK_KING_CASTLED  = (1ULL << 6)  | (1ULL << 2);
    constexpr uint64_t BLACK_ROOK_CASTLED  = (1ULL << 5)  | (1ULL << 3);

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
            const int sq = poplsbIndex(pawns);
            d.pawnAttacks |= pieces::PAWN_ATTACKS[side][sq];
        }
        d.allAttacks = d.pawnAttacks;

        // Knights - usa lookup table (no magic bitboards)
        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = poplsbIndex(knights);
            const uint64_t attacks = pieces::KNIGHT_ATTACKS[sq];
            d.knightAttacks |= attacks;
            d.knightMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.knightAttacks;

        // Bishops - magic bitboards necessari
        uint64_t bishops = b.bishops_bb[side];
        while (bishops) {
            const int sq = poplsbIndex(bishops);
            const uint64_t attacks = pieces::getBishopAttacks(sq, occ);
            d.bishopAttacks |= attacks;
            d.bishopMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.bishopAttacks;

        // Rooks - magic bitboards necessari
        uint64_t rooks = b.rooks_bb[side];
        while (rooks) {
            const int sq = poplsbIndex(rooks);
            const uint64_t attacks = pieces::getRookAttacks(sq, occ);
            d.rookAttacks |= attacks;
            d.rookMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.rookAttacks;

        // Queens - magic bitboards necessari
        uint64_t queens = b.queens_bb[side];
        while (queens) {
            const int sq = poplsbIndex(queens);
            const uint64_t attacks = pieces::getQueenAttacks(sq, occ);
            d.queenAttacks |= attacks;
            d.queenMobility += __builtin_popcountll(attacks & ~occ);
        }
        d.allAttacks |= d.queenAttacks;
    }
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
    // PRECOMPUTE ATTACK DATA (used by multiple evaluations)
    // ===================================================
    AttackData attackData[2];
    computeAttackData(attackData, board, occ);

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
        
        // Basic piece safety (avoid hanging pieces)
        eval += evalHangingPieces(board, attackData);
        
        // Center control è FONDAMENTALE in opening
        eval += evalCentralControl(whitePawns, blackPawns);
        
        // Knight positioning (avoid rim)
        eval += evalKnightOnRim(board);
        
        // Basic pawn structure (non troppo dettagliato)
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        
        // Mobility bonus (sviluppare pezzi = più mosse)
        eval += evalMobility(attackData);
        
        // Initiative bonus (side to move advantage)
        eval += evalInitiative(board, false);
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
        
        // Full tactical evaluation
        eval += evalHangingPieces(board, attackData);
        eval += evalTrappedPieces(board, occ);
        
        // Pawn structure becomes more important
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        eval += evalCentralControl(whitePawns, blackPawns);
        
        // Piece activity
        eval += evalMobility(attackData);
        eval += evalKnightOnRim(board);
        eval += evalBadBishop(board.bishops_bb[0], whitePawns, 0);
        eval += evalBadBishop(board.bishops_bb[1], blackPawns, 1);
        
        // King safety starts to matter
        eval += evalKingSafety(board, whitePawns, blackPawns);
        eval += evalBadKingPosition(board);
        
        // Rook evaluation
        eval += evalRooks(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
        
        // Initiative
        eval += evalInitiative(board, false);
    }
    
    // ===================================================
    // MIDDLEGAME (moves 15+, many pieces on board)
    // Focus: tactics, king safety, piece coordination
    // ===================================================
    else if (isMiddlegame) {
        // Full tactical evaluation (molto importante!)
        eval += evalHangingPieces(board, attackData);
        eval += evalTrappedPieces(board, occ);
        
        // Pawn structure evaluation
        eval += evalPawnStructure(whitePawns, blackPawns, false);
        eval += evalCentralControl(whitePawns, blackPawns);
        eval += evalBlockedCenterWithPieces(board, occ);
        
        // Piece activity and positioning
        eval += evalMobility(attackData);
        eval += evalKnightOnRim(board);
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
    }
    
    // ===================================================
    // ENDGAME (few pieces left)
    // Focus: pawn promotion, king activity, passed pawns
    // ===================================================
    else { // isEndgame
        // Tactical safety still matters
        eval += evalHangingPieces(board, attackData);
        
        // Pawn structure è CRITICO in endgame
        eval += evalPawnStructure(whitePawns, blackPawns, true);
        
        // King activity è fondamentale
        eval += evalKingActivity(board, true);
        eval += evalEndgameKingActivity(board);
        
        // Piece mobility
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

bool Engine::isMate() noexcept {
    uint8_t toMove = board.getActiveColor();
    return board.kings_bb[0] == 0    || board.kings_bb[1] == 0 
        || board.isCheckmate(toMove) || board.isDraw(toMove);
}

void Engine::setIsCheckMate() noexcept {
    isCheckMate = isMate();
}

}; // namespace engine
