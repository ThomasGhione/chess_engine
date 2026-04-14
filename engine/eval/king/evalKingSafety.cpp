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
    const bool onCastlingSquare = (side == 0) ? (sq == 62 || sq == 58) : (sq == 6 || sq == 2);
    const bool hasCastled = onCastlingSquare && rightsLost;

    applyNonCastledPenalties(b, side, rightsLost, hasCastled, canCastleKingside, canCastleQueenside, whitePawns, blackPawns, sideSafety, sq);
    applyKingShieldSupport(side, sq, whitePawns, blackPawns, sideSafety);

    const bool kingSideRelevant = canCastleKingside || kingFile >= 5;
    applyHookPawnPenalty(b, side, kingSideRelevant, ownPawns, ownAttacks, enemyAttacks, sideSafety);

    applyShelterAndStorm(b, side, kingFile, kingRank, ownPawns, enemyPawns, hasCastled, enemyHeavyPieces, sideSafety);
    applyOpenDiagonalPenalty(b, side, kingFile, kingRank, sideColor, sideSafety);

    return sign * sideSafety;
}

inline void Evaluator::applyNonCastledPenalties(const chess::Board&, int side, bool rightsLost, bool hasCastled,
                                                bool canCastleKingside, bool canCastleQueenside,
                                                uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety, int sq) noexcept {
    const uint64_t ownPawns = (side == 0) ? whitePawns : blackPawns;
    const uint64_t KS_SHIELD = (side == 0) ? 0x00E0000000000000ULL : 0x000000000000E000ULL;
    const uint64_t QS_SHIELD = (side == 0) ? 0x000E000000000000ULL : 0x0000000000000E00ULL;

    if (!hasCastled) {
        sideSafety -= engine::KING_NON_CASTLING_PENALTY;
        if (rightsLost) {
            sideSafety -= engine::KING_LOST_CASTLING_RIGHTS_PENALTY;
        }

        if (canCastleKingside) sideSafety -= std::popcount(KS_SHIELD & ~ownPawns) * 16;
        if (canCastleQueenside) sideSafety -= std::popcount(QS_SHIELD & ~ownPawns) * 16;
    } else {
        const bool isKs = (side == 0) ? (sq == 62) : (sq == 6);
        const bool isQs = (side == 0) ? (sq == 58) : (sq == 2);
        const uint64_t castledShieldMask = (isKs ? KS_SHIELD : 0ULL) | (isQs ? QS_SHIELD : 0ULL);
        sideSafety -= std::popcount(castledShieldMask & ~ownPawns) * engine::KING_CASTLED_SHIELD_BREAK_PENALTY;
    }
}

inline void Evaluator::applyKingShieldSupport(int side, int sq, uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept {
    const uint64_t kingBB = 1ULL << sq;
    if (side == 0) {
        const uint64_t shieldSquares = (kingBB >> 8) | ((kingBB & ~0x8080808080808080ULL) >> 7) | ((kingBB & ~0x0101010101010101ULL) >> 9);
        sideSafety += std::popcount(whitePawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
    } else {
        const uint64_t shieldSquares = (kingBB << 8) | ((kingBB & ~0x8080808080808080ULL) << 9) | ((kingBB & ~0x0101010101010101ULL) << 7);
        sideSafety += std::popcount(blackPawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
    }
}

inline void Evaluator::applyHookPawnPenalty(const chess::Board&, int side, bool kingSideRelevant, uint64_t ownPawns,
                                            uint64_t ownAttacks, uint64_t enemyAttacks, int32_t& sideSafety) noexcept {
    if (!kingSideRelevant) return;

    const uint8_t hookPawnSq = (side == 0) ? 54 : 14;
    const uint64_t hookPawnBit = chess::Board::bitMask(hookPawnSq);
    if (ownPawns & hookPawnBit) {
        if (enemyAttacks & hookPawnBit) {
            sideSafety -= engine::KING_HOOK_PAWN_ATTACKED_PENALTY;
            if ((ownAttacks & hookPawnBit) == 0ULL) {
                sideSafety -= engine::KING_HOOK_PAWN_HANGING_PENALTY;
            }
        }
    }
}

inline void Evaluator::applyShelterAndStorm(const chess::Board&, int side, int kingFile, int kingRank,
                                            uint64_t ownPawns, uint64_t enemyPawns, bool hasCastled,
                                            const uint64_t enemyHeavyPieces, int32_t& sideSafety) noexcept {
    const uint64_t rankBelowMask = (1ULL << (kingRank * 8)) - 1;
    const uint64_t rankAboveMask = kingRank == 7 ? 0ULL : (~0ULL << ((kingRank + 1) * 8));
    const uint64_t inFrontMask = side == 0 ? rankBelowMask : rankAboveMask;

    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
        const uint64_t fileMask = FILE_MASKS[f];
        const bool ownPawnOnFile = (ownPawns & fileMask) != 0ULL;
        const bool enemyPawnOnFile = (enemyPawns & fileMask) != 0ULL;
        const bool isKingFile = (f == kingFile);

        int shelterDist = 99;
        uint64_t ownInFront = ownPawns & fileMask & inFrontMask;
        if (ownInFront) {
            if (side == 0) {
                int pawnSq = 63 - __builtin_clzll(ownInFront);
                shelterDist = kingRank - chess::Board::rank(pawnSq);
            } else {
                int pawnSq = __builtin_ctzll(ownInFront);
                shelterDist = chess::Board::rank(pawnSq) - kingRank;
            }
        }

        if (shelterDist == 1) {
            sideSafety += engine::KING_SHELTER_STRONG_BONUS;
        } else if (shelterDist == 2) {
            sideSafety += engine::KING_SHELTER_WEAK_BONUS;
        } else if (shelterDist == 3) {
            sideSafety += engine::KING_SHELTER_WEAK_BONUS / 2;
        } else {
            sideSafety -= engine::KING_SHELTER_MISSING_PENALTY;
        }

        if (hasCastled && shelterDist >= 2 && shelterDist < 99) {
            int advancePenalty = 0;
            if (shelterDist == 2) {
                advancePenalty = engine::KING_SHELTER_ADVANCE_ONE_PENALTY;
            } else {
                advancePenalty = engine::KING_SHELTER_ADVANCE_TWO_PENALTY;
                advancePenalty += std::min(4, std::max(0, shelterDist - 3)) * 2;
            }

            if (isKingFile) {
                advancePenalty += 2;
            }
            sideSafety -= advancePenalty;
        }

        int stormDist = 99;
        uint64_t enemyInFront = enemyPawns & fileMask & inFrontMask;
        if (enemyInFront) {
            if (side == 0) {
                int pawnSq = 63 - __builtin_clzll(enemyInFront);
                stormDist = kingRank - chess::Board::rank(pawnSq);
            } else {
                int pawnSq = __builtin_ctzll(enemyInFront);
                stormDist = chess::Board::rank(pawnSq) - kingRank;
            }
        }

        if (stormDist <= 2) {
            sideSafety -= engine::KING_PAWN_STORM_NEAR_PENALTY
                + ((shelterDist >= 4) ? 4 : 0);
        } else if (stormDist <= 4) {
            sideSafety -= engine::KING_PAWN_STORM_FAR_PENALTY;
        }

        if (!ownPawnOnFile) {
            int filePenalty = enemyPawnOnFile
                ? engine::KING_SEMI_OPEN_FILE_PENALTY
                : engine::KING_OPEN_FILE_PENALTY;

            if (isKingFile) {
                filePenalty += filePenalty / 2;
            }
            sideSafety -= filePenalty;

            if (enemyHeavyPieces & fileMask) {
                sideSafety -= engine::KING_FILE_PRESSURE_PENALTY + (isKingFile ? 4 : 0);
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

    int hits = std::popcount(attacks);
    sideSafety -= hits * engine::KING_OPEN_DIAGONAL_PENALTY;

    while (attacks) {
        int hitSq = popLSB(attacks);
        int dist = std::max(std::abs(kingFile - (hitSq & 7)), std::abs(kingRank - (hitSq >> 3)));
        if (dist <= 2) {
            sideSafety -= engine::KING_OPEN_DIAGONAL_PENALTY / 2;
        }
    }
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
