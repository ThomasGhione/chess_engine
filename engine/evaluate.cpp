#include "engine.hpp"

namespace engine {
    
int64_t Engine::getMaterialDeltaFAST(const chess::Board& b) noexcept {
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
        uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbWhite));
        bbWhite &= (bbWhite - 1);
        eval += table[sq];
    }
    // Black pieces: mirror index vertically
    while (bbBlack) {
        uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbBlack));
        bbBlack &= (bbBlack - 1);
        uint8_t idx = mirrorIndex(sq);
        eval -= table[idx];
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


int64_t Engine::evaluatePawnStructureFast(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    // NOTE: this is intentionally cheap. The previous implementation had bugs:
    // - doubled pawn detection used (1ULL << file) which is not a file mask
    // - isolated pawn checked against the wrong side's pawn set
    // - passed pawn mask was invalid/overflow-prone
    int64_t score = 0;

    // Doubled pawns: count per-file pawn excess over 1.
    for (int f = 0; f < 8; ++f) {
        const uint64_t fm = fileMask(f);
        const int wCount = __builtin_popcountll(whitePawns & fm);
        const int bCount = __builtin_popcountll(blackPawns & fm);
        if (wCount > 1) score += (wCount - 1) * DOUBLED_PAWN_PENALTY;
        if (bCount > 1) score -= (bCount - 1) * DOUBLED_PAWN_PENALTY;
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
        if ((blackPawns & lanes & inFront) == 0) score += PASSED_PAWN_BONUS;
    }
    bp = blackPawns;
    while (bp) {
        const int sq = poplsbIndex(bp);
        const int file = sq & 7;
        const uint64_t lanes = fileMask(file) | adjacentFilesMask(file);
        // Squares in front of sq for black are ranks < current rank.
        const int rank = sq >> 3;
        const uint64_t inFront = (rank == 0) ? 0ULL : ((1ULL << (rank * 8)) - 1ULL);
        if ((whitePawns & lanes & inFront) == 0) score -= PASSED_PAWN_BONUS;
    }

    return score;
}

int64_t Engine::evaluatePassedPawnScalingFast(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    int64_t score = 0;

    uint64_t wp = whitePawns;
    while (wp) {
        const int sq = poplsbIndex(wp);
        const int rank = sq >> 3;
        score += (rank - 1) * 6;
    }

    uint64_t bp = blackPawns;
    while (bp) {
        const int sq = poplsbIndex(bp);
        const int rank = 7 - (sq >> 3);
        score -= (rank - 1) * 6;
    }

    return score;
}


int64_t Engine::evaluateRooksFast(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    int64_t score = 0;

    auto evalSide = [&](uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns, int sign) {
        while (rooks) {
            const int sq = poplsbIndex(rooks);
            const int file = sq & 7;
            const int rank = sq >> 3;
            const uint64_t fm = fileMask(file);

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


int64_t Engine::evaluateMobilityFast(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;
    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        uint64_t knights = b.knights_bb[side];
        uint64_t bishops  = b.bishops_bb[side];
        uint64_t rooks    = b.rooks_bb[side];
        uint64_t queens   = b.queens_bb[side];

        while (knights) {
            const int sq = poplsbIndex(knights);
            score += sign * __builtin_popcountll(pieces::KNIGHT_ATTACKS[sq] & ~occ);
        }
        while (bishops) {
            const int sq = poplsbIndex(bishops);
            score += sign * __builtin_popcountll(pieces::getBishopAttacks(sq, occ) & ~occ);
        }
        while (rooks) {
            const int sq = poplsbIndex(rooks);
            score += sign * __builtin_popcountll(pieces::getRookAttacks(sq, occ) & ~occ);
        }
        while (queens) {
            const int sq = poplsbIndex(queens);
            score += sign * __builtin_popcountll(pieces::getQueenAttacks(sq, occ) & ~occ);
        }
    }
    return score / 2; // normalizzazione
}

int64_t Engine::evaluateInitiativeFast(const chess::Board& b, bool isEndgame) noexcept {
    // piccolo bonus al side to move
    constexpr int64_t INIT_BONUS_MG = 12;
    constexpr int64_t INIT_BONUS_EG = 4;

    const int64_t bonus = isEndgame ? INIT_BONUS_EG : INIT_BONUS_MG;
    return (b.getActiveColor() == chess::Board::WHITE) ? bonus : -bonus;
}

int64_t Engine::evaluateBadBishopFast(uint64_t bishops, uint64_t pawns, int side) noexcept {
    // Old version was O(#bishops * #pawns) due to scanning pawns for each bishop.
    // Keep it O(#bishops + #pawns) by counting pawns on light/dark once.
    uint64_t pawnDark = 0;
    uint64_t pawnLight = 0;
    {
        uint64_t p = pawns;
        while (p) {
            const int psq = poplsbIndex(p);
            const bool isDark = ((psq ^ (psq >> 3)) & 1);
            if (isDark) pawnDark++;
            else pawnLight++;
        }
    }

    int64_t score = 0;
    while (bishops) {
        const int sq = poplsbIndex(bishops);
        const bool bishopOnDark = ((sq ^ (sq >> 3)) & 1);
        const uint64_t sameColorPawns = bishopOnDark ? pawnDark : pawnLight;
        score -= static_cast<int64_t>(sameColorPawns) * 5; // tuning safe
    }
    return (side == 0) ? score : -score;
}


int64_t Engine::evaluateEarlyQueenFast(const chess::Board& b) noexcept {
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

int64_t Engine::evaluateTrappedPiecesFast(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;

        uint64_t knights = b.knights_bb[side];
        uint64_t bishops = b.bishops_bb[side];
        uint64_t rooks  = b.rooks_bb[side];
        uint64_t queens = b.queens_bb[side];

        while (knights) {
            constexpr int64_t LOW_MOBILITY_KNIGHT_VALUE_PENALTY = 10;
            constexpr int64_t PINNED_KNIGHT_VALUE_PENALTY = 60;
            const int sq = poplsbIndex(knights);
            const int mobility = __builtin_popcountll((pieces::KNIGHT_ATTACKS[sq]) & ~occ);
            if (mobility == 0) score -= sign * PINNED_KNIGHT_VALUE_PENALTY;
            if (mobility <= 3) score -= sign * LOW_MOBILITY_KNIGHT_VALUE_PENALTY;
        }

        while (bishops) {
            constexpr int64_t LOW_MOBILITY_BISHOP_VALUE_PENALTY = 20;
            constexpr int64_t PINNED_BISHOP_VALUE_PENALTY = 40;
            const int sq = poplsbIndex(bishops);
            const int mobility = __builtin_popcountll((pieces::getBishopAttacks(sq, occ)) & ~occ);
            if (mobility == 0) score -= sign * PINNED_BISHOP_VALUE_PENALTY;
            else if (mobility <= 3) score -= sign * LOW_MOBILITY_BISHOP_VALUE_PENALTY;
        }

        while (rooks) {
            constexpr int64_t LOW_MOBILITY_ROOK_VALUE_PENALTY = 30;
            constexpr int64_t PINNED_ROOK_VALUE_PENALTY = 30;
            const int sq = poplsbIndex(rooks);
            const int mobility = __builtin_popcountll(pieces::getRookAttacks(sq, occ) & ~occ);
            if (mobility == 0) score -= sign * PINNED_ROOK_VALUE_PENALTY;
            else if (mobility <= 3) score -= sign * LOW_MOBILITY_ROOK_VALUE_PENALTY;
        }

        while (queens) {
            constexpr int64_t LOW_MOBILITY_QUEEN_VALUE_PENALTY = 60;
            constexpr int64_t PINNED_QUEEN_VALUE_PENALTY = 200;
            const int sq = poplsbIndex(queens);
            const int mobility = __builtin_popcountll(pieces::getQueenAttacks(sq, occ) & ~occ);
            if (mobility == 0) score -= sign * PINNED_QUEEN_VALUE_PENALTY;
            else if (mobility <= 3) score -= sign * LOW_MOBILITY_QUEEN_VALUE_PENALTY;
        }
    }

    return score;
}

int64_t Engine::evaluateHangingPiecesFast(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        const int opp  = side ^ 1;

        // -------------------------------------------------
        // 1) ALL ENEMY ATTACKS (cheap first)
        // -------------------------------------------------
        uint64_t enemyAttacks = 0ULL;

        // Pawns
        uint64_t pawns = b.pawns_bb[opp];
        while (pawns) {
            const int sq = poplsbIndex(pawns);
            enemyAttacks |= pieces::PAWN_ATTACKS[opp][sq];
        }

        // Knights
        uint64_t knights = b.knights_bb[opp];
        while (knights) {
            enemyAttacks |= pieces::KNIGHT_ATTACKS[poplsbIndex(knights)];
        }

        // Bishops
        uint64_t bishops = b.bishops_bb[opp];
        while (bishops) {
            const int sq = poplsbIndex(bishops);
            enemyAttacks |= pieces::getBishopAttacks(sq, occ);
        }

        // Rooks
        uint64_t rooks = b.rooks_bb[opp];
        while (rooks) {
            const int sq = poplsbIndex(rooks);
            enemyAttacks |= pieces::getRookAttacks(sq, occ);
        }

        // Queens
        uint64_t queens = b.queens_bb[opp];
        while (queens) {
            const int sq = poplsbIndex(queens);
            enemyAttacks |= pieces::getQueenAttacks(sq, occ);
        }

        // -------------------------------------------------
        // 2) ALL FRIENDLY DEFENSES
        // -------------------------------------------------
        uint64_t friendlyDef = 0ULL;

        pawns = b.pawns_bb[side];
        while (pawns) {
            const int sq = poplsbIndex(pawns);
            friendlyDef |= pieces::PAWN_ATTACKS[side][sq];
        }

        knights = b.knights_bb[side];
        while (knights) {
            friendlyDef |= pieces::KNIGHT_ATTACKS[poplsbIndex(knights)];
        }

        bishops = b.bishops_bb[side];
        while (bishops) {
            const int sq = poplsbIndex(bishops);
            friendlyDef |= pieces::getBishopAttacks(sq, occ);
        }

        rooks = b.rooks_bb[side];
        while (rooks) {
            const int sq = poplsbIndex(rooks);
            friendlyDef |= pieces::getRookAttacks(sq, occ);
        }

        queens = b.queens_bb[side];
        while (queens) {
            const int sq = poplsbIndex(queens);
            friendlyDef |= pieces::getQueenAttacks(sq, occ);
        }

        // -------------------------------------------------
        // 3) HANGING PIECES (branchless-ish)
        // -------------------------------------------------
        uint64_t hanging;

        hanging = b.pawns_bb[side] & enemyAttacks & ~friendlyDef;
        score -= sign * __builtin_popcountll(hanging) * 20;

        hanging = b.knights_bb[side] & enemyAttacks & ~friendlyDef;
        score -= sign * __builtin_popcountll(hanging) * 50;

        hanging = b.bishops_bb[side] & enemyAttacks & ~friendlyDef;
        score -= sign * __builtin_popcountll(hanging) * 40;

        hanging = b.rooks_bb[side] & enemyAttacks & ~friendlyDef;
        score -= sign * __builtin_popcountll(hanging) * 60;

        hanging = b.queens_bb[side] & enemyAttacks & ~friendlyDef;
        score -= sign * __builtin_popcountll(hanging) * 100;
    }

    return score;
}

int64_t Engine::evaluateCentralControlFast(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    constexpr uint64_t CENTER_MASK = 0x0000001818000000ULL; // e4,d4,e5,d5
    return (__builtin_popcountll(whitePawns & CENTER_MASK) - __builtin_popcountll(blackPawns & CENTER_MASK)) * 5;
}

int64_t Engine::evaluateKingSafetyFast(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
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

int64_t Engine::evaluateKingActivityFast(const chess::Board& b, bool isEndgame) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int sign = (side == 0) ? 1 : -1;
        const uint64_t kingBB = b.kings_bb[side];
        if (!kingBB) [[unlikely]] continue;

        const int ksq = __builtin_ctzll(kingBB);

        // ENDGAME: king activity
        if (isEndgame) {
            uint64_t friends =
                b.pawns_bb[side]   |
                b.knights_bb[side] |
                b.bishops_bb[side] |
                b.rooks_bb[side]   |
                b.queens_bb[side];

            while (friends) {
                const int sq = poplsbIndex(friends);
                if (manhattan(ksq, sq) <= 2)
                    score += sign * KING_ACTIVITY_BONUS;
            }
        }

        // MIDGAME: enemy proximity penalty
        uint64_t enemies =
            b.pawns_bb[side ^ 1]   |
            b.knights_bb[side ^ 1] |
            b.bishops_bb[side ^ 1] |
            b.rooks_bb[side ^ 1]   |
            b.queens_bb[side ^ 1];

        while (enemies) {
            const int sq = poplsbIndex(enemies);
            if (manhattan(ksq, sq) <= 2)
                score += sign * KING_SAFETY_PENALTY;
        }
    }

    return score;
}

int64_t Engine::evaluateBadKingPositionFast(const chess::Board& b) noexcept {
    constexpr int64_t KING_EXPOSED_PENALTY = -120;

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


int64_t Engine::evaluateEndgameKingActivityFast(const chess::Board& b) noexcept {
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




int64_t Engine::evaluate(const chess::Board& board) noexcept {
    if (board.isCheckmate(board.getActiveColor())) [[unlikely]] {
        return evaluateCheckmate(board);
    }

    int64_t eval = getMaterialDeltaFAST(board);

    const uint64_t occ = board.getPiecesBitMap();
    const uint64_t whitePawns = board.pawns_bb[0];
    const uint64_t blackPawns = board.pawns_bb[1];

    // Endgame flag
    int nonPawnMajors = __builtin_popcountll(board.knights_bb[0] | board.knights_bb[1] |
                                             board.bishops_bb[0] | board.bishops_bb[1] |
                                             board.rooks_bb[0]   | board.rooks_bb[1]   |
                                             board.queens_bb[0]  | board.queens_bb[1]);
    bool isEndgame = (nonPawnMajors <= PHASE_FINAL_THRESHOLD);

    // Positional evaluation using piece-square tables
    addPsqt(board.pawns_bb[0], board.pawns_bb[1], (isEndgame ? PAWN_END_GAME_VALUES_TABLE : PAWN_VALUES_TABLE).data(), eval);
    addPsqt(board.knights_bb[0], board.knights_bb[1], KNIGHT_VALUES_TABLE.data(), eval);
    addPsqt(board.bishops_bb[0], board.bishops_bb[1], BISHOP_VALUES_TABLE.data(), eval);
    addPsqt(board.rooks_bb[0],   board.rooks_bb[1],   ROOK_VALUES_TABLE.data(), eval);
    addPsqt(board.queens_bb[0],  board.queens_bb[1],  QUEEN_VALUES_TABLE.data(), eval);
    addPsqt(board.kings_bb[0],   board.kings_bb[1],   (isEndgame ? KING_END_GAME_VALUES_TABLE : KING_MIDDLE_GAME_VALUES_TABLE).data(), eval);


    // OPENING SPECIFIC EVALUATIONS

    if (board.getFullMoveClock() < 8) {
        eval += evaluateEarlyQueenFast(board);
    }
    else {
        eval += evaluateBadBishopFast(board.bishops_bb[0], whitePawns, 0);
        eval += evaluateBadBishopFast(board.bishops_bb[1], blackPawns, 1);
    }

    // MIDDLEGAME & ENDGAME EVALUATIONS
    // NOTE: mobility + trapped pieces are among the most expensive parts (sliding attacks).
    // Keep them, but we'll also add a separate fast path below for callers that want max speed.
    eval += evaluatePawnStructureFast(whitePawns, blackPawns);
    eval += evaluateMobilityFast(board, occ);
    eval += evaluateInitiativeFast(board, isEndgame);
    eval += evaluateTrappedPiecesFast(board, occ);
    eval += evaluateRooksFast(board.rooks_bb[0], board.rooks_bb[1], whitePawns, blackPawns);
    eval += evaluateKingActivityFast(board, isEndgame);
    eval += evaluateHangingPiecesFast(board, occ);

    if (!isEndgame) { // MIDDLEGAME SPECIFIC EVALUATIONS
        eval += evaluateKingSafetyFast(board, whitePawns, blackPawns);
        eval += evaluateCentralControlFast(whitePawns, blackPawns);
        eval += evaluateBadKingPositionFast(board);
    }
    else { // ENDGAME SPECIFIC EVALUATIONS
        eval += evaluatePassedPawnScalingFast(whitePawns, blackPawns);
        eval += evaluateEndgameKingActivityFast(board);
        
    }

    // TODO cambiare in una funzione
    // Castling bonus (bitmask)
    if ((board.kings_bb[0] & ((1ULL << 62) | (1ULL << 58))) && 
        (board.rooks_bb[0] & ((1ULL << 61) | (1ULL << 59)))) eval += CASTLING_BONUS;
    if ((board.kings_bb[1] & ((1ULL << 6) | (1ULL << 2))) && 
        (board.rooks_bb[1] & ((1ULL << 5) | (1ULL << 3)))) eval -= CASTLING_BONUS;



    return eval;
}

bool Engine::isMate() noexcept {
    uint8_t toMove = this->board.getActiveColor();
    if (this->board.isCheckmate(toMove) || this->board.isStalemate(toMove)) {
        return true;
    }
    return false;
}


int64_t Engine::getMaterialDelta(const chess::Board& b) noexcept {

	static constexpr auto coefficientPiece = [](uint8_t piece) {
	    return -2 * static_cast<int64_t>(piece >> 3) + 1;
  };

  static constexpr auto pieceValue = [](uint8_t x) {
    const int64_t x64 = static_cast<int64_t>(x);  // Cast PRIMA delle moltiplicazioni
    return (x64 * (-134220 +
      x64 * (304540 +
      x64 * (-240405 +
      x64 * (87775 +
      x64 * (-15075 +
      x64 * 985)))))) / 36;
  };

  int64_t delta = 0;
  static constexpr uint8_t MAX_INDEX = 64;
  //#pragma omp parallel for reduction(+:delta)
  for (uint8_t i = 0; i < MAX_INDEX; i++) {
    uint8_t piece = b.get(i);

    delta += coefficientPiece(piece & chess::Board::MASK_COLOR) * pieceValue(piece & chess::Board::MASK_PIECE_TYPE);
  }
  
  return delta;
}

}; // namespace engine
