#include <bit>
#include "../evaluator.hpp"

namespace engine {

inline PhaseValue Evaluator::evalKingSafetySide(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2],
                                                 bool whiteCastleKs, bool whiteCastleQs, bool blackCastleKs, bool blackCastleQs, int side,
                                                 int32_t materialScale) noexcept {
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return {};

    const int sq = std::countr_zero(kingBB);
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
        enemyAttacks |= pieces::KING_ATTACKS[std::countr_zero(oppKingBB)];
    }
    const uint64_t enemyHeavyPieces = b.rooks_bb[opp] | b.queens_bb[opp];
    const uint8_t sideColor = (side == 0) ? chess::Board::WHITE : chess::Board::BLACK;
    PhaseValue sideSafety{};

    const bool rightsLost = !canCastleKingside && !canCastleQueenside;
    const bool kingOnWing = kingFile <= 2 || kingFile >= 5;

    // Each helper takes int32_t& and modifies a scalar via PhaseValue.mg/.eg
    // semantics implemented inline below; the PhaseValue accumulation is done
    // here.
    int32_t mgSafety = 0;
    int32_t egSafety = 0;

    auto add = [&](PhaseValue pv) { mgSafety += pv.mg; egSafety += pv.eg; };
    auto sub = [&](PhaseValue pv) { mgSafety -= pv.mg; egSafety -= pv.eg; };

    // --- applyNonCastledPenalties ---
    {
        const uint64_t KS_SHIELD = (side == 0) ? 0x00E0000000000000ULL : 0x000000000000E000ULL;
        const uint64_t QS_SHIELD = (side == 0) ? 0x000E000000000000ULL : 0x0000000000000E00ULL;
        const bool canCastle = canCastleKingside || canCastleQueenside;

        if (!kingOnWing && (rightsLost || canCastle)) {
            sub(KING_NON_CASTLING_PENALTY);
            if (rightsLost) sub(KING_LOST_CASTLING_RIGHTS_PENALTY);
        }
        if (canCastleKingside) {
            const int n = std::popcount(KS_SHIELD & ~ownPawns);
            mgSafety -= n * KING_SHELTER_PAWN_MULTIPLIER;
            egSafety -= n * KING_SHELTER_PAWN_MULTIPLIER;
        }
        if (canCastleQueenside) {
            const int n = std::popcount(QS_SHIELD & ~ownPawns);
            mgSafety -= n * KING_SHELTER_PAWN_MULTIPLIER;
            egSafety -= n * KING_SHELTER_PAWN_MULTIPLIER;
        }
    }

    // --- applyKingShieldSupport ---
    {
        const uint64_t shieldSquares = pieces::KING_ATTACKS[sq] &
                                       (side == 0 ? WHITE_FORWARD_FILL[sq] : BLACK_FORWARD_FILL[sq]);
        const int n = std::popcount(((side == 0) ? whitePawns : blackPawns) & shieldSquares);
        add(n * CASTLE_PAWN_SUPPORT_BONUS);
    }

    // --- applyHookPawnPenalty ---
    {
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
                sub(KING_HOOK_PAWN_ATTACKED_PENALTY);
                if ((ownAttacks & hookPawnBit) == 0ULL) {
                    sub(KING_HOOK_PAWN_HANGING_PENALTY);
                }
            }
        }
    }

    // --- applyShelterAndStorm ---
    {
        const uint64_t rankBelowMask = RANK_BELOW_MASKS[kingRank];
        const uint64_t rankAboveMask = RANK_ABOVE_MASKS[kingRank];
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
                    ? (63 - std::countl_zero(ownInFront))
                    : std::countr_zero(ownInFront);
                shelterDist = std::abs(kingRank - (pawnSq >> 3));
            }

            if (shelterDist == KING_SHELTER_VERY_CLOSE) {
                add(KING_SHELTER_STRONG_BONUS);
            } else if (shelterDist == KING_SHELTER_CLOSE) {
                add(KING_SHELTER_WEAK_BONUS);
            } else if (shelterDist == KING_SHELTER_FAR) {
                add(PhaseValue{KING_SHELTER_WEAK_BONUS.mg / 2, KING_SHELTER_WEAK_BONUS.eg / 2});
            } else {
                sub(KING_SHELTER_MISSING_PENALTY);
            }

            if (kingOnWing && shelterDist >= KING_SHELTER_MIN_ADVANCE_CHECK && shelterDist < KING_SHELTER_INIT_DISTANCE) {
                PhaseValue advancePenalty = (shelterDist == KING_SHELTER_CLOSE)
                    ? KING_SHELTER_ADVANCE_ONE_PENALTY
                    : (KING_SHELTER_ADVANCE_TWO_PENALTY + PhaseValue{std::min(4, shelterDist - 3) * KING_SHELTER_ADVANCE_PAWN_MULTIPLIER});
                if (isKingFile) {
                    advancePenalty += PhaseValue{2};
                }
                sub(advancePenalty);
            }

            int stormDist = 99;
            uint64_t enemyInFront = enemyPawns & fileMask & inFrontMask;
            if (enemyInFront) {
                int pawnSq = (side == 0)
                    ? (63 - std::countl_zero(enemyInFront))
                    : std::countr_zero(enemyInFront);
                stormDist = std::abs(kingRank - (pawnSq >> 3));
            }

            if (stormDist <= 2) {
                sub(KING_PAWN_STORM_NEAR_PENALTY + PhaseValue{(shelterDist >= 4) ? 4 : 0});
            } else if (stormDist <= 4) {
                sub(KING_PAWN_STORM_FAR_PENALTY);
            }

            if (!ownPawnOnFile) {
                PhaseValue filePenalty = enemyPawnOnFile
                    ? KING_SEMI_OPEN_FILE_PENALTY
                    : KING_OPEN_FILE_PENALTY;
                if (isKingFile) {
                    filePenalty += PhaseValue{filePenalty.mg / 2, filePenalty.eg / 2};
                }
                sub(filePenalty);

                if (enemyHeavyPieces & fileMask) {
                    sub(KING_FILE_PRESSURE_PENALTY + PhaseValue{isKingFile ? 4 : 0});
                }
            }
        }
    }

    // --- applyOpenDiagonalPenalty ---
    {
        const int sqLocal = (kingRank << 3) | kingFile;
        const int oppLocal = sideColor == chess::Board::WHITE ? 1 : 0;
        const uint64_t enemyBishopsQueens = b.bishops_bb[oppLocal] | b.queens_bb[oppLocal];
        if (enemyBishopsQueens) {
            uint64_t attacks = pieces::getBishopAttacks(sqLocal, b.getPiecesBitMap()) & enemyBishopsQueens;
            if (attacks) {
                int hits = std::popcount(attacks);
                sub(hits * KING_OPEN_DIAGONAL_PENALTY);
                int closeHits = std::popcount(attacks & KING_PROXIMITY_MASKS[sqLocal]);
                sub(closeHits * PhaseValue{KING_OPEN_DIAGONAL_PENALTY.mg / 2, KING_OPEN_DIAGONAL_PENALTY.eg / 2});
            }
        }
    }

    // Scale danger (negative scores) by material scale percent.
    if (mgSafety < 0) mgSafety = (mgSafety * materialScale) / 100;
    if (egSafety < 0) egSafety = (egSafety * materialScale) / 100;

    mgSafety = std::clamp(mgSafety, -KING_SAFETY_SIDE_CAP, KING_SAFETY_SIDE_CAP);
    egSafety = std::clamp(egSafety, -KING_SAFETY_SIDE_CAP, KING_SAFETY_SIDE_CAP);

    // Down-material damping: an attack mounted while behind in non-pawn material
    // rarely converts (the defender can hand material back to blunt it). Applied
    // AFTER the cap so it still bites when the raw danger saturates the cap —
    // the sacrifice case, where stripping the enemy king reads as a big gain.
    // opp is the attacker; `side` is the defended king's owner.
    if (mgSafety < 0 || egSafety < 0) {
        const int attMaterial = 3 * std::popcount(b.knights_bb[opp] | b.bishops_bb[opp])
                              + 5 * std::popcount(b.rooks_bb[opp])
                              + 9 * std::popcount(b.queens_bb[opp]);
        const int defMaterial = 3 * std::popcount(b.knights_bb[side] | b.bishops_bb[side])
                              + 5 * std::popcount(b.rooks_bb[side])
                              + 9 * std::popcount(b.queens_bb[side]);
        const int materialDeficit = defMaterial - attMaterial;  // attacker behind
        if (materialDeficit > 0) {
            const int factor = std::max(0, 100 - materialDeficit * KING_ATTACK_DOWN_MATERIAL_PENALTY);
            if (mgSafety < 0) mgSafety = (mgSafety * factor) / 100;
            if (egSafety < 0) egSafety = (egSafety * factor) / 100;
        }
    }

    sideSafety.mg = mgSafety;
    sideSafety.eg = egSafety;
    return sign * sideSafety;
}


int32_t Evaluator::attackMaterialScalePercent(const chess::Board& b, int attackingSide, int targetKingFile,
                                               uint64_t targetPawns) noexcept {
    if (targetKingFile < 0 || targetKingFile > 7) [[unlikely]] return KING_ATTACK_MATERIAL_MIN_SCALE;

    const int queenCount = std::popcount(b.queens_bb[attackingSide]);
    const int rookCount = std::popcount(b.rooks_bb[attackingSide]);
    const int minorCount = std::popcount(b.knights_bb[attackingSide] | b.bishops_bb[attackingSide]);

    int32_t scale = KING_ATTACK_MATERIAL_MIN_SCALE
                  + queenCount * KING_ATTACK_QUEEN_WEIGHT
                  + rookCount * KING_ATTACK_ROOK_WEIGHT
                  + minorCount * KING_ATTACK_MINOR_WEIGHT;

    const uint64_t attackingHeavy = b.rooks_bb[attackingSide] | b.queens_bb[attackingSide];
    for (int f = std::max(0, targetKingFile - 1); f <= std::min(7, targetKingFile + 1); ++f) {
        const uint64_t fileMask = FILE_MASKS[f];
        if ((targetPawns & fileMask) == 0ULL) {
            scale += KING_ATTACK_OPEN_FILE_INCREMENT;
            if (attackingHeavy & fileMask) scale += KING_ATTACK_HEAVY_FILE_INCREMENT;
        }
    }

    return std::clamp(scale, KING_ATTACK_MATERIAL_MIN_SCALE, KING_ATTACK_MATERIAL_MAX_SCALE);
}

inline int32_t Evaluator::scaleKingDanger(int32_t value, int32_t scalePercent) noexcept {
    if (value < 0) return (value * scalePercent) / 100;
    return value;
}

PhaseValue Evaluator::evalKingSafetyWithAttackData(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    if (!b.kings_bb[0] || !b.kings_bb[1]) [[unlikely]] return {};

    const bool whiteCastleKs = b.getCastle(0);
    const bool whiteCastleQs = b.getCastle(1);
    const bool blackCastleKs = b.getCastle(2);
    const bool blackCastleQs = b.getCastle(3);

    const int whiteKingFile = chess::Board::file(std::countr_zero(b.kings_bb[0]));
    const int blackKingFile = chess::Board::file(std::countr_zero(b.kings_bb[1]));
    const int32_t scaleBlackAttackingWhite = attackMaterialScalePercent(b, 1, whiteKingFile, whitePawns);
    const int32_t scaleWhiteAttackingBlack = attackMaterialScalePercent(b, 0, blackKingFile, blackPawns);

    return evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 0, scaleBlackAttackingWhite)
         + evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 1, scaleWhiteAttackingBlack);
}

PhaseValue Evaluator::evalKingMiddlegame(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    if (!b.kings_bb[0] || !b.kings_bb[1]) [[unlikely]] return {};

    const bool whiteCastleKs = b.getCastle(0);
    const bool whiteCastleQs = b.getCastle(1);
    const bool blackCastleKs = b.getCastle(2);
    const bool blackCastleQs = b.getCastle(3);

    const int whiteKingFile = chess::Board::file(std::countr_zero(b.kings_bb[0]));
    const int blackKingFile = chess::Board::file(std::countr_zero(b.kings_bb[1]));
    const int32_t scaleBlackAttackingWhite = attackMaterialScalePercent(b, 1, whiteKingFile, whitePawns);
    const int32_t scaleWhiteAttackingBlack = attackMaterialScalePercent(b, 0, blackKingFile, blackPawns);

    const PhaseValue safety =
        evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 0, scaleBlackAttackingWhite)
      + evalKingSafetySide(b, whitePawns, blackPawns, data, whiteCastleKs, whiteCastleQs, blackCastleKs, blackCastleQs, 1, scaleWhiteAttackingBlack);

    const uint64_t occ = b.getPiecesBitMap();
    const int32_t attackZone =
        evalKingAttackZoneSide(b, data, 0, occ, scaleWhiteAttackingBlack)
      + evalKingAttackZoneSide(b, data, 1, occ, scaleBlackAttackingWhite);

    // Attack zone is an mg-side concept (king safety matters in MG, not EG).
    return safety + PhaseValue{attackZone, 0};
}

PhaseValue Evaluator::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const uint64_t occ = b.getPiecesBitMap();
    AttackData attackData[2];
    computeAttackData(attackData, b, occ);
    return evalKingSafetyWithAttackData(b, whitePawns, blackPawns, attackData);
}

} // namespace engine
