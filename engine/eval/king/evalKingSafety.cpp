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
}

inline void Evaluator::applyKingShieldSupport(int side, int sq, uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept {
    uint64_t shieldSquares = 0ULL;
    const int kingFile = chess::Board::file(sq);
    if (side == 0) {
        if (sq >= 8) shieldSquares |= chess::Board::bitMask(sq - 8);
        if (sq >= 7 && kingFile != 7) shieldSquares |= chess::Board::bitMask(sq - 7);
        if (sq >= 9 && kingFile != 0) shieldSquares |= chess::Board::bitMask(sq - 9);
        sideSafety += __builtin_popcountll(whitePawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
    } else {
        if (sq <= 55) shieldSquares |= chess::Board::bitMask(sq + 8);
        if (sq <= 56 && kingFile != 0) shieldSquares |= chess::Board::bitMask(sq + 7);
        if (sq <= 54 && kingFile != 7) shieldSquares |= chess::Board::bitMask(sq + 9);
        sideSafety += __builtin_popcountll(blackPawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
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
    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
        const uint64_t fileMask = FILE_MASKS[f];
        const bool ownPawnOnFile = (ownPawns & fileMask) != 0ULL;
        const bool enemyPawnOnFile = (enemyPawns & fileMask) != 0ULL;
        const bool isKingFile = (f == kingFile);

        int shelterDist = 99;
        uint64_t ownFilePawns = ownPawns & fileMask;
        while (ownFilePawns) {
            const int pawnSq = popLSB(ownFilePawns);
            const int pawnRank = chess::Board::rank(pawnSq);
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
        uint64_t enemyFilePawns = enemyPawns & fileMask;
        while (enemyFilePawns) {
            const int pawnSq = popLSB(enemyFilePawns);
            const int pawnRank = chess::Board::rank(pawnSq);
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
    static constexpr int DIAG_DIRS[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    for (const auto& dir : DIAG_DIRS) {
        int f = kingFile + dir[0];
        int r = kingRank + dir[1];
        int rayDist = 1;
        while (static_cast<unsigned>(f) < 8U && static_cast<unsigned>(r) < 8U) {
            const uint8_t raySq = (r << 3) | f;
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
                int diagPenalty = engine::KING_OPEN_DIAGONAL_PENALTY;
                if (rayDist <= 2) {
                    diagPenalty += engine::KING_OPEN_DIAGONAL_PENALTY / 2;
                }
                sideSafety -= diagPenalty;
            }
            break;
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
