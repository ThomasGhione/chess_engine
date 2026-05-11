#include "../evaluator.hpp"

namespace engine {

inline int32_t Evaluator::evalKingSafetySide(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2],
                                             bool whiteCastleKs, bool whiteCastleQs, bool blackCastleKs, bool blackCastleQs, int side) noexcept {
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return 0;

    const int sq = __builtin_ctzll(kingBB);
    const int kingFile = chess::Board::file(sq);
    const int kingRank = chess::Board::rank(sq);
    const int sign = (side == 0) ? 1 : -1;
    const int opp = side ^ 1;
    const bool canCastleKingside = (side == 0) ? whiteCastleKs : blackCastleKs;
    const bool canCastleQueenside = (side == 0) ? whiteCastleQs : blackCastleQs;
    const uint64_t ownPawns = (side == 0) ? whitePawns : blackPawns;
    const uint64_t enemyPawns = (side == 0) ? blackPawns : whitePawns;
    const uint64_t oppKingBB = b.kings_bb[opp];
    uint64_t ownAttacks = data[side].allAttacks | pieces::KING_ATTACKS[sq];
    uint64_t enemyAttacks = data[opp].allAttacks;
    if (oppKingBB) {
        enemyAttacks |= pieces::KING_ATTACKS[__builtin_ctzll(oppKingBB)];
    }
    const uint64_t enemyHeavyPieces = b.rooks_bb[opp] | b.queens_bb[opp];
    const uint8_t sideColor = (side == 0) ? chess::Board::WHITE : chess::Board::BLACK;
    int32_t sideSafety = 0;

    const bool rightsLost = !canCastleKingside && !canCastleQueenside;
    const bool kingOnWing = kingFile <= 2 || kingFile >= 5;

    applyNonCastledPenalties(b, side, rightsLost, kingOnWing, canCastleKingside, canCastleQueenside, whitePawns, blackPawns, sideSafety);
    applyKingShieldSupport(side, sq, whitePawns, blackPawns, sideSafety);

    applyHookPawnPenalty(b, side, kingFile, ownPawns, ownAttacks, enemyAttacks, sideSafety);

    applyShelterAndStorm(b, side, kingFile, kingRank, ownPawns, enemyPawns, kingOnWing, enemyHeavyPieces, sideSafety);
    applyOpenDiagonalPenalty(b, side, kingFile, kingRank, sideColor, sideSafety);

    sideSafety = scaleKingDanger(sideSafety, attackMaterialScalePercent(b, opp, kingFile, ownPawns));
    sideSafety = std::clamp(sideSafety, -KING_SAFETY_SIDE_CAP, KING_SAFETY_SIDE_CAP);

    return sign * sideSafety;
}

int32_t Evaluator::attackMaterialScalePercent(const chess::Board& b, int attackingSide, int targetKingFile,
                                              uint64_t targetPawns) noexcept {
    const int queenCount = __builtin_popcountll(b.queens_bb[attackingSide]);
    const int rookCount = __builtin_popcountll(b.rooks_bb[attackingSide]);
    const int minorCount = __builtin_popcountll(b.knights_bb[attackingSide] | b.bishops_bb[attackingSide]);

    int32_t scale = KING_ATTACK_MATERIAL_MIN_SCALE
                  + queenCount * KING_ATTACK_QUEEN_WEIGHT
                  + rookCount * KING_ATTACK_ROOK_WEIGHT
                  + minorCount * KING_ATTACK_MINOR_WEIGHT;

    const uint64_t attackingHeavy = b.rooks_bb[attackingSide] | b.queens_bb[attackingSide];
    for (int f = std::max(0, targetKingFile - 1); f <= std::min(7, targetKingFile + 1); ++f) {
        const uint64_t fileMask = FILE_MASKS[f];
        if ((targetPawns & fileMask) == 0ULL) {
            scale += KING_ATTACK_OPEN_FILE_INCREMENT;
            if (attackingHeavy & fileMask) {
                scale += KING_ATTACK_HEAVY_FILE_INCREMENT;
            }
        }
    }

    return std::clamp(scale, KING_ATTACK_MATERIAL_MIN_SCALE, KING_ATTACK_MATERIAL_MAX_SCALE);
}

inline int32_t Evaluator::scaleKingDanger(int32_t value, int32_t scalePercent) noexcept {
    if (value < 0) {
        return (value * scalePercent) / 100;
    }
    return value;
}

inline void Evaluator::applyNonCastledPenalties(const chess::Board&, int side, bool rightsLost, bool kingOnWing,
                                                bool canCastleKingside, bool canCastleQueenside,
                                                uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept {
    const uint64_t ownPawns = (side == 0) ? whitePawns : blackPawns;
    const uint64_t KS_SHIELD = (side == 0) ? 0x00E0000000000000ULL : 0x000000000000E000ULL;
    const uint64_t QS_SHIELD = (side == 0) ? 0x000E000000000000ULL : 0x0000000000000E00ULL;
    const bool canCastle = canCastleKingside || canCastleQueenside;

    if (!kingOnWing && (rightsLost || canCastle)) {
        sideSafety -= KING_NON_CASTLING_PENALTY;
        if (rightsLost) {
            sideSafety -= KING_LOST_CASTLING_RIGHTS_PENALTY;
        }
    }

    if (canCastleKingside) sideSafety -= __builtin_popcountll(KS_SHIELD & ~ownPawns) * KING_SHELTER_PAWN_MULTIPLIER;
    if (canCastleQueenside) sideSafety -= __builtin_popcountll(QS_SHIELD & ~ownPawns) * KING_SHELTER_PAWN_MULTIPLIER;
}

inline void Evaluator::applyKingShieldSupport(int side, int sq, uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept {
    if (side == 0) {
        const uint64_t shieldSquares = pieces::KING_ATTACKS[sq] & WHITE_FORWARD_FILL[sq];
        sideSafety += __builtin_popcountll(whitePawns & shieldSquares) * CASTLE_PAWN_SUPPORT_BONUS;
    } else {
        const uint64_t shieldSquares = pieces::KING_ATTACKS[sq] & BLACK_FORWARD_FILL[sq];
        sideSafety += __builtin_popcountll(blackPawns & shieldSquares) * CASTLE_PAWN_SUPPORT_BONUS;
    }
}

inline void Evaluator::applyHookPawnPenalty(const chess::Board&, int side, int kingFile, uint64_t ownPawns,
                                            uint64_t ownAttacks, uint64_t enemyAttacks, int32_t& sideSafety) noexcept {
    const int homePawnRank = (side == 0) ? 6 : 1;
    const int outerFile = (kingFile >= 4) ? 7 : 0;
    const int hookFile = (kingFile >= 4) ? 6 : 1;
    uint64_t hookPawns = chess::Board::bitMask((homePawnRank << 3) | hookFile)
                       | chess::Board::bitMask((homePawnRank << 3) | outerFile);
    hookPawns &= ownPawns;

    while (hookPawns) {
        const uint8_t hookPawnSq = popLSB(hookPawns);
        const uint64_t hookPawnBit = chess::Board::bitMask(hookPawnSq);
        if (enemyAttacks & hookPawnBit) {
            sideSafety -= KING_HOOK_PAWN_ATTACKED_PENALTY;
            if ((ownAttacks & hookPawnBit) == 0ULL) {
                sideSafety -= KING_HOOK_PAWN_HANGING_PENALTY;
            }
        }
    }
}

inline void Evaluator::applyShelterAndStorm(const chess::Board&, int side, int kingFile, int kingRank,
                                            uint64_t ownPawns, uint64_t enemyPawns, bool kingOnWing,
                                            const uint64_t enemyHeavyPieces, int32_t& sideSafety) noexcept {
    const uint64_t rankBelowMask = (1ULL << (kingRank * 8)) - 1;
    const uint64_t rankAboveMask = kingRank == 7 ? 0ULL : (~0ULL << ((kingRank + 1) * 8));
    const uint64_t inFrontMask = side == 0 ? rankBelowMask : rankAboveMask;

    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
        const uint64_t fileMask = FILE_MASKS[f];
        const bool ownPawnOnFile = (ownPawns & fileMask) != 0ULL;
        const bool enemyPawnOnFile = (enemyPawns & fileMask) != 0ULL;
        const bool isKingFile = (f == kingFile);

        int shelterDist = KING_SHELTER_INIT_DISTANCE;
        uint64_t ownInFront = ownPawns & fileMask & inFrontMask;
        if (ownInFront) {
            int pawnSq = (side == 0) 
                ? (63 - __builtin_clzll(ownInFront)) 
                : __builtin_ctzll(ownInFront);
            shelterDist = std::abs(kingRank - (pawnSq >> 3));
        }

        if (shelterDist == KING_SHELTER_VERY_CLOSE) {
            sideSafety += KING_SHELTER_STRONG_BONUS;
        } else if (shelterDist == KING_SHELTER_CLOSE) {
            sideSafety += KING_SHELTER_WEAK_BONUS;
        } else if (shelterDist == KING_SHELTER_FAR) {
            sideSafety += KING_SHELTER_WEAK_BONUS / 2;
        } else {
            sideSafety -= KING_SHELTER_MISSING_PENALTY;
        }

        if (kingOnWing && shelterDist >= KING_SHELTER_MIN_ADVANCE_CHECK && shelterDist < KING_SHELTER_INIT_DISTANCE) {
            int advancePenalty = (shelterDist == KING_SHELTER_CLOSE) 
                ? KING_SHELTER_ADVANCE_ONE_PENALTY 
                : (KING_SHELTER_ADVANCE_TWO_PENALTY + std::min(4, shelterDist - 3) * KING_SHELTER_ADVANCE_PAWN_MULTIPLIER);
            if (isKingFile) {
                advancePenalty += 2;
            }
            sideSafety -= advancePenalty;
        }

        int stormDist = 99;
        uint64_t enemyInFront = enemyPawns & fileMask & inFrontMask;
        if (enemyInFront) {
            int pawnSq = (side == 0) 
                ? (63 - __builtin_clzll(enemyInFront)) 
                : __builtin_ctzll(enemyInFront);
            stormDist = std::abs(kingRank - (pawnSq >> 3));
        }

        if (stormDist <= 2) {
            sideSafety -= KING_PAWN_STORM_NEAR_PENALTY + ((shelterDist >= 4) ? 4 : 0);
        } else if (stormDist <= 4) {
            sideSafety -= KING_PAWN_STORM_FAR_PENALTY;
        }

        if (!ownPawnOnFile) {
            int filePenalty = enemyPawnOnFile
                ? KING_SEMI_OPEN_FILE_PENALTY
                : KING_OPEN_FILE_PENALTY;
            if (isKingFile) {
                filePenalty += filePenalty / 2;
            }
            sideSafety -= filePenalty;

            if (enemyHeavyPieces & fileMask) {
                sideSafety -= KING_FILE_PRESSURE_PENALTY + (isKingFile ? 4 : 0);
            }
        }
    }
}

inline void Evaluator::applyOpenDiagonalPenalty(const chess::Board& b, int, int kingFile, int kingRank, uint8_t sideColor, int32_t& sideSafety) noexcept {
    const int sq = (kingRank << 3) | kingFile;
    const int opp = sideColor == chess::Board::WHITE ? 1 : 0;
    const uint64_t enemyBishopsQueens = b.bishops_bb[opp] | b.queens_bb[opp];
    if (!enemyBishopsQueens) return;

    uint64_t attacks = pieces::getBishopAttacks(sq, b.getPiecesBitMap()) & enemyBishopsQueens;
    if (!attacks) return;

    int hits = __builtin_popcountll(attacks);
    sideSafety -= hits * KING_OPEN_DIAGONAL_PENALTY;

    int closeHits = __builtin_popcountll(attacks & KING_PROXIMITY_MASKS[sq]);
    sideSafety -= closeHits * (KING_OPEN_DIAGONAL_PENALTY / 2);
}

int32_t Evaluator::evalKingSafetyWithAttackData(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    const bool whiteCastleKs = b.getCastle(0);
    const bool whiteCastleQs = b.getCastle(1);
    const bool blackCastleKs = b.getCastle(2);
    const bool blackCastleQs = b.getCastle(3);

    return evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 0)
         + evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 1);
}

int32_t Evaluator::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const uint64_t occ = b.getPiecesBitMap();
    AttackData attackData[2];
    computeAttackData(attackData, b, occ);
    return evalKingSafetyWithAttackData(b, whitePawns, blackPawns, attackData);
}

} // namespace engine
