#include "../evaluator.hpp"

namespace engine {

int32_t Evaluator::evalKingSafetyWithAttackData(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept {
    int32_t score = 0;
    const bool whiteCastleKs = b.getCastle(0);
    const bool whiteCastleQs = b.getCastle(1);
    const bool blackCastleKs = b.getCastle(2);
    const bool blackCastleQs = b.getCastle(3);

    for (int side = 0; side < 2; ++side) {
        const uint64_t kingBB = b.kings_bb[side];
        if (!kingBB) [[unlikely]] continue;

        const int sq = __builtin_ctzll(kingBB);
        const int kingFile = chess::Board::fileOf(sq);
        const int kingRank = chess::Board::rankOf(sq);
        const int sign = (side == 0) ? 1 : -1;
        const int opp = side ^ 1;
        const bool canCastleKingside = (side == 0) ? whiteCastleKs : blackCastleKs;
        const bool canCastleQueenside = (side == 0) ? whiteCastleQs : blackCastleQs;
        const uint64_t ownPawns = (side == 0) ? whitePawns : blackPawns;
        const uint64_t enemyPawns = (side == 0) ? blackPawns : whitePawns;
        uint64_t ownAttacks = data[side].allAttacks;
        uint64_t enemyAttacks = data[opp].allAttacks;
        if (b.kings_bb[side]) {
            ownAttacks |= pieces::KING_ATTACKS[__builtin_ctzll(b.kings_bb[side])];
        }
        if (b.kings_bb[opp]) {
            enemyAttacks |= pieces::KING_ATTACKS[__builtin_ctzll(b.kings_bb[opp])];
        }
        const uint64_t enemyHeavyPieces = b.rooks_bb[opp] | b.queens_bb[opp];
        const uint8_t sideColor = (side == 0) ? chess::Board::WHITE : chess::Board::BLACK;
        int32_t sideSafety = 0;

        const bool rightsLost = !canCastleKingside && !canCastleQueenside;
        const bool onCastlingSquare = (side == 0) ? (sq == 62 || sq == 58) : (sq == 6 || sq == 2);
        const bool hasCastled = onCastlingSquare && rightsLost;

        if (!hasCastled) {
            sideSafety -= engine::KING_NON_CASTLING_PENALTY;
            if (rightsLost) {
                sideSafety -= engine::KING_LOST_CASTLING_RIGHTS_PENALTY;
            }

            if (side == 0) {
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(53) | chess::Board::bitMask(54) | chess::Board::bitMask(55);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~whitePawns);
                    sideSafety -= movedPawns * 16;
                }

                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(49) | chess::Board::bitMask(50) | chess::Board::bitMask(51);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~whitePawns);
                    sideSafety -= movedPawns * 16;
                }
            } else {
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(13) | chess::Board::bitMask(14) | chess::Board::bitMask(15);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~blackPawns);
                    sideSafety -= movedPawns * 16;
                }

                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(9) | chess::Board::bitMask(10) | chess::Board::bitMask(11);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~blackPawns);
                    sideSafety -= movedPawns * 16;
                }
            }
        } else {
            // After castling, pushing the original pawn shield should carry an explicit cost.
            uint64_t castledShieldMask = 0ULL;
            if (side == 0) {
                if (sq == 62) {
                    castledShieldMask = chess::Board::bitMask(53) | chess::Board::bitMask(54) | chess::Board::bitMask(55);
                } else if (sq == 58) {
                    castledShieldMask = chess::Board::bitMask(49) | chess::Board::bitMask(50) | chess::Board::bitMask(51);
                }
                const int movedShieldPawns = __builtin_popcountll(castledShieldMask & ~whitePawns);
                sideSafety -= movedShieldPawns * engine::KING_CASTLED_SHIELD_BREAK_PENALTY;
            } else {
                if (sq == 6) {
                    castledShieldMask = chess::Board::bitMask(13) | chess::Board::bitMask(14) | chess::Board::bitMask(15);
                } else if (sq == 2) {
                    castledShieldMask = chess::Board::bitMask(9) | chess::Board::bitMask(10) | chess::Board::bitMask(11);
                }
                const int movedShieldPawns = __builtin_popcountll(castledShieldMask & ~blackPawns);
                sideSafety -= movedShieldPawns * engine::KING_CASTLED_SHIELD_BREAK_PENALTY;
            }
        }

        uint64_t shieldSquares = 0ULL;
        if (side == 0) {
            if (sq >= 8) shieldSquares |= chess::Board::bitMask(sq - 8);
            if (sq >= 7 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq - 7);
            if (sq >= 9 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq - 9);
            sideSafety += __builtin_popcountll(whitePawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
        } else {
            if (sq <= 55) shieldSquares |= chess::Board::bitMask(sq + 8);
            if (sq <= 56 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq + 7);
            if (sq <= 54 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq + 9);
            sideSafety += __builtin_popcountll(blackPawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
        }

        const bool kingSideRelevant = canCastleKingside || kingFile >= 5;
        if (kingSideRelevant) {
            const uint8_t hookPawnSq = static_cast<uint8_t>((side == 0) ? 54 : 14); // g2 / g7
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

        // Shelter/storm + open file model on king file and adjacent files.
        for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
            const uint64_t fileMask = FILE_MASKS[f];
            const bool ownPawnOnFile = (ownPawns & fileMask) != 0ULL;
            const bool enemyPawnOnFile = (enemyPawns & fileMask) != 0ULL;
            const bool isKingFile = (f == kingFile);

            int shelterDist = 99;
            uint64_t ownFilePawns = ownPawns & fileMask;
            while (ownFilePawns) {
                const int pawnSq = popLSB(ownFilePawns);
                const int pawnRank = chess::Board::rankOf(pawnSq);
                if (side == 0) {
                    if (pawnRank < kingRank) {
                        shelterDist = std::min(shelterDist, kingRank - pawnRank);
                    }
                } else {
                    if (pawnRank > kingRank) {
                        shelterDist = std::min(shelterDist, pawnRank - kingRank);
                    }
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

             // Avoid overvaluing own pawn storms when they come from weakening a castled king shell.
            if (hasCastled && shelterDist >= 2 && shelterDist < 99) {
                int advancePenalty = 0;
                if (shelterDist == 2) {
                    advancePenalty = static_cast<int>(engine::KING_SHELTER_ADVANCE_ONE_PENALTY);
                } else {
                    advancePenalty = static_cast<int>(engine::KING_SHELTER_ADVANCE_TWO_PENALTY);
                    advancePenalty += std::min(4, std::max(0, shelterDist - 3)) * 2;
                }

                if (isKingFile) {
                    advancePenalty += 2;
                }
                sideSafety -= advancePenalty;
            }

            int stormDist = 99;
            uint64_t enemyFilePawns = enemyPawns & fileMask;
            while (enemyFilePawns) {
                const int pawnSq = popLSB(enemyFilePawns);
                const int pawnRank = chess::Board::rankOf(pawnSq);
                if (side == 0) {
                    if (pawnRank < kingRank) {
                        stormDist = std::min(stormDist, kingRank - pawnRank);
                    }
                } else {
                    if (pawnRank > kingRank) {
                        stormDist = std::min(stormDist, pawnRank - kingRank);
                    }
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
                    ? static_cast<int>(engine::KING_SEMI_OPEN_FILE_PENALTY)
                    : static_cast<int>(engine::KING_OPEN_FILE_PENALTY);

                if (isKingFile) {
                    filePenalty += filePenalty / 2;
                }
                sideSafety -= filePenalty;

                if (enemyHeavyPieces & fileMask) {
                    sideSafety -= engine::KING_FILE_PRESSURE_PENALTY + (isKingFile ? 4 : 0);
                }
            }
        }

        // Open diagonals toward king: if the first blocker on a diagonal ray is
        // an enemy bishop/queen, king shelter is considered compromised.
        static constexpr int DIAG_DIRS[4][2] = {
            {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
        };
        for (const auto& dir : DIAG_DIRS) {
            int f = kingFile + dir[0];
            int r = kingRank + dir[1];
            int rayDist = 1;
            while (static_cast<unsigned>(f) < 8U && static_cast<unsigned>(r) < 8U) {
                const uint8_t raySq = static_cast<uint8_t>((r << 3) | f);
                const uint8_t piece = b.get(raySq);
                if (piece == chess::Board::EMPTY) {
                    f += dir[0];
                    r += dir[1];
                    ++rayDist;
                    continue;
                }

                if ((piece & chess::Board::MASK_COLOR) == sideColor) {
                    break;
                }

                const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
                if (pieceType == chess::Board::BISHOP || pieceType == chess::Board::QUEEN) {
                    int diagPenalty = static_cast<int>(engine::KING_OPEN_DIAGONAL_PENALTY);
                    if (rayDist <= 2) {
                        diagPenalty += static_cast<int>(engine::KING_OPEN_DIAGONAL_PENALTY / 2);
                    }
                    sideSafety -= diagPenalty;
                }
                break;
            }
        }

        score += sign * sideSafety;
    }

    return score;
}

int32_t Evaluator::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    const uint64_t occ = b.getPiecesBitMap();
    AttackData attackData[2];
    computeAttackData(attackData, b, occ);
    return evalKingSafetyWithAttackData(b, whitePawns, blackPawns, attackData);
}

int32_t Evaluator::evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept {
    int32_t score = 0;
    const uint64_t occ = b.getPiecesBitMap();

    static constexpr int ATTACKER_SCALE_PERCENT[9] = {0, 0, 32, 52, 68, 80, 90, 97, 100};

    for (int side = 0; side < 2; ++side) {
        const int oppSide = side ^ 1;
        const int sign = (side == 0) ? 1 : -1;

        const uint64_t enemyKingBB = b.kings_bb[oppSide];
        if (!enemyKingBB) [[unlikely]] continue;

        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const uint64_t kingZone = pieces::KING_ATTACKS[enemyKingSq] | chess::Board::bitMask(enemyKingSq);

        constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL;
        constexpr uint64_t BLACK_MINOR_START = 0x000000000000FFFFULL;

        const uint64_t developedKnights = (side == 0)
            ? (b.knights_bb[side] & ~WHITE_MINOR_START)
            : (b.knights_bb[side] & ~BLACK_MINOR_START);
        const uint64_t developedBishops = (side == 0)
            ? (b.bishops_bb[side] & ~WHITE_MINOR_START)
            : (b.bishops_bb[side] & ~BLACK_MINOR_START);

        int attackerCount = 0;
        int32_t attackWeight = 0;

        accumulateKingZoneAttackers<knightAttacksLookup, engine::KING_ATTACK_WEIGHT_KNIGHT>(
            developedKnights, kingZone, occ, attackerCount, attackWeight);
        accumulateKingZoneAttackers<pieces::getBishopAttacks, engine::KING_ATTACK_WEIGHT_BISHOP>(
            developedBishops, kingZone, occ, attackerCount, attackWeight);
        accumulateKingZoneAttackers<pieces::getRookAttacks, engine::KING_ATTACK_WEIGHT_ROOK>(
            b.rooks_bb[side], kingZone, occ, attackerCount, attackWeight);
        accumulateKingZoneAttackers<pieces::getQueenAttacks, engine::KING_ATTACK_WEIGHT_QUEEN>(
            b.queens_bb[side], kingZone, occ, attackerCount, attackWeight);

        if (attackerCount >= 2) {
            const uint64_t defenderMap = data[oppSide].allAttacks | pieces::KING_ATTACKS[enemyKingSq];
            const uint64_t zoneAttacks = data[side].allAttacks & kingZone;
            const int safeContacts = __builtin_popcountll(zoneAttacks & ~defenderMap);
            const int forcingContacts = __builtin_popcountll(zoneAttacks & defenderMap);

            int32_t attackUnits = attackWeight
                + safeContacts * engine::KING_SAFE_CONTACT_BONUS
                + forcingContacts * engine::KING_FORCING_CONTACT_BONUS;

            const uint64_t knightChecks = b.knights_bb[side] & pieces::KNIGHT_ATTACKS[enemyKingSq];
            const uint64_t bishopChecks = b.bishops_bb[side] & pieces::getBishopAttacks(enemyKingSq, occ);
            const uint64_t rookChecks = b.rooks_bb[side] & pieces::getRookAttacks(enemyKingSq, occ);
            const uint64_t queenChecks = b.queens_bb[side] & pieces::getQueenAttacks(enemyKingSq, occ);

            addKingCheckUnits(knightChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS, engine::KING_FORCING_CHECK_BONUS, attackUnits);
            addKingCheckUnits(bishopChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS, engine::KING_FORCING_CHECK_BONUS, attackUnits);
            addKingCheckUnits(rookChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS + 4, engine::KING_FORCING_CHECK_BONUS + 2, attackUnits);
            addKingCheckUnits(queenChecks, defenderMap, engine::KING_SAFE_CHECK_BONUS + 8, engine::KING_FORCING_CHECK_BONUS + 4, attackUnits);

            const int scaleIndex = std::min(attackerCount, 8);
            int32_t attackDanger = (attackUnits * ATTACKER_SCALE_PERCENT[scaleIndex]) / 100;
            attackDanger = std::min<int32_t>(attackDanger, engine::KING_ATTACK_DANGER_CAP);

            score += sign * attackDanger;
        }
    }

    return score;
}

int32_t Evaluator::evalCastlingBonus(const chess::Board& b) noexcept {
    // Castled detection: king on a castling destination square AND both castling
    // rights lost.  The rook may have moved away from its post-castling square
    // (e.g. Rf1-e1), but the king position + lost rights proves castling occurred.
    // This matches the same hasCastled logic used in evalKingSafety.
    static constexpr uint64_t WHITE_KING_CASTLED  = (chess::Board::bitMask(62) | chess::Board::bitMask(58));
    static constexpr uint64_t BLACK_KING_CASTLED  = (chess::Board::bitMask(6)  | chess::Board::bitMask(2));

    int32_t score = 0;
    const bool whiteCastleKs = b.getCastle(0);
    const bool whiteCastleQs = b.getCastle(1);
    const bool blackCastleKs = b.getCastle(2);
    const bool blackCastleQs = b.getCastle(3);

    const bool whiteRightsLost = !whiteCastleKs && !whiteCastleQs;
    const bool whiteHasCastled = (b.kings_bb[0] & WHITE_KING_CASTLED) && whiteRightsLost;
    const bool whiteCanCastle = whiteCastleKs || whiteCastleQs;

    score += whiteHasCastled * engine::CASTLING_BONUS;
    score -= (!whiteHasCastled && !whiteCanCastle) * engine::LOSS_OF_CASTLING_PENALTY;

    const bool blackRightsLost = !blackCastleKs && !blackCastleQs;
    const bool blackHasCastled = (b.kings_bb[1] & BLACK_KING_CASTLED) && blackRightsLost;
    const bool blackCanCastle = blackCastleKs || blackCastleQs;

    score -= blackHasCastled * engine::CASTLING_BONUS;
    score += (!blackHasCastled && !blackCanCastle) * engine::LOSS_OF_CASTLING_PENALTY;

    return score;
}

} // namespace engine
