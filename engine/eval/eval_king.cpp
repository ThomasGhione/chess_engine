#include "evaluator.hpp"

namespace engine {

int64_t Evaluator::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const uint64_t kingBB = b.kings_bb[side];
        if (!kingBB) [[unlikely]] continue;
        const int sq = __builtin_ctzll(kingBB);
        const int sign = (side == 0) ? 1 : -1;

        const bool rightsLost = !b.getCastle(side == 0 ? 0 : 2) && !b.getCastle(side == 0 ? 1 : 3);
        const bool onCastlingSquare = (side == 0) ? (sq == 62 || sq == 58) : (sq == 6 || sq == 2);
        const bool hasCastled = onCastlingSquare && rightsLost;

        if (!hasCastled) {
            score += sign * (-engine::KING_NON_CASTLING_PENALTY);

            const bool canCastleKingside = (side == 0) ? b.getCastle(0) : b.getCastle(2);
            const bool canCastleQueenside = (side == 0) ? b.getCastle(1) : b.getCastle(3);

            if (side == 0) {
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(53) | chess::Board::bitMask(54) | chess::Board::bitMask(55);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~whitePawns);
                    score -= movedPawns * 12;
                }

                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(49) | chess::Board::bitMask(50) | chess::Board::bitMask(51);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~whitePawns);
                    score -= movedPawns * 12;
                }
            } else {
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(13) | chess::Board::bitMask(14) | chess::Board::bitMask(15);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~blackPawns);
                    score += movedPawns * 12;
                }

                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(9) | chess::Board::bitMask(10) | chess::Board::bitMask(11);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~blackPawns);
                    score += movedPawns * 12;
                }
            }
        }

        uint64_t shieldSquares = 0ULL;
        if (side == 0) {
            if (sq >= 8) shieldSquares |= chess::Board::bitMask(sq - 8);
            if (sq >= 7 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq - 7);
            if (sq >= 9 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq - 9);
            score += sign * __builtin_popcountll(whitePawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
        } else {
            if (sq <= 55) shieldSquares |= chess::Board::bitMask(sq + 8);
            if (sq <= 56 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq + 7);
            if (sq <= 54 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq + 9);
            score += sign * __builtin_popcountll(blackPawns & shieldSquares) * engine::CASTLE_PAWN_SUPPORT_BONUS;
        }
    }

    return score;
}

int64_t Evaluator::evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept {
    (void)data;
    int64_t score = 0;

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
        int64_t attackWeight = 0;

        {
            uint64_t knightsAttacking = developedKnights;
            while (knightsAttacking) {
                const int sq = __builtin_ctzll(knightsAttacking);
                knightsAttacking &= knightsAttacking - 1;
                if (pieces::KNIGHT_ATTACKS[sq] & kingZone) {
                    attackerCount++;
                    attackWeight += engine::KING_ATTACK_WEIGHT_KNIGHT;
                }
            }
        }

        {
            uint64_t bishopsAttacking = developedBishops;
            while (bishopsAttacking) {
                const int sq = __builtin_ctzll(bishopsAttacking);
                bishopsAttacking &= bishopsAttacking - 1;
                const uint64_t occ = b.getPiecesBitMap();
                if (pieces::getBishopAttacks(sq, occ) & kingZone) {
                    attackerCount++;
                    attackWeight += engine::KING_ATTACK_WEIGHT_BISHOP;
                }
            }
        }

        {
            uint64_t rooksAttacking = b.rooks_bb[side];
            while (rooksAttacking) {
                const int sq = __builtin_ctzll(rooksAttacking);
                rooksAttacking &= rooksAttacking - 1;
                const uint64_t occ = b.getPiecesBitMap();
                if (pieces::getRookAttacks(sq, occ) & kingZone) {
                    attackerCount++;
                    attackWeight += engine::KING_ATTACK_WEIGHT_ROOK;
                }
            }
        }

        {
            uint64_t queensAttacking = b.queens_bb[side];
            while (queensAttacking) {
                const int sq = __builtin_ctzll(queensAttacking);
                queensAttacking &= queensAttacking - 1;
                const uint64_t occ = b.getPiecesBitMap();
                if (pieces::getQueenAttacks(sq, occ) & kingZone) {
                    attackerCount++;
                    attackWeight += engine::KING_ATTACK_WEIGHT_QUEEN;
                }
            }
        }

        if (attackerCount >= 2) {
            const int64_t attackDanger = (attackerCount * attackerCount * attackWeight) / 12;
            score += sign * attackDanger;
        }
    }

    return score;
}

int64_t Evaluator::evalCastlingBonus(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_KING_CASTLED  = (chess::Board::bitMask(62) | chess::Board::bitMask(58));
    static constexpr uint64_t WHITE_ROOK_CASTLED  = (chess::Board::bitMask(61) | chess::Board::bitMask(59));
    static constexpr uint64_t BLACK_KING_CASTLED  = (chess::Board::bitMask(6)  | chess::Board::bitMask(2));
    static constexpr uint64_t BLACK_ROOK_CASTLED  = (chess::Board::bitMask(5)  | chess::Board::bitMask(3));
    static constexpr int64_t LOSS_OF_CASTLING_PENALTY = 60;

    int64_t score = 0;

    const bool whiteHasCastled = (b.kings_bb[0] & WHITE_KING_CASTLED) && (b.rooks_bb[0] & WHITE_ROOK_CASTLED);
    const bool whiteCanCastle = b.getCastle(0) || b.getCastle(1);

    score += whiteHasCastled * engine::CASTLING_BONUS;
    score -= (!whiteHasCastled && !whiteCanCastle) * LOSS_OF_CASTLING_PENALTY;

    const bool blackHasCastled = (b.kings_bb[1] & BLACK_KING_CASTLED) && (b.rooks_bb[1] & BLACK_ROOK_CASTLED);
    const bool blackCanCastle = b.getCastle(2) || b.getCastle(3);

    score -= blackHasCastled * engine::CASTLING_BONUS;
    score += (!blackHasCastled && !blackCanCastle) * LOSS_OF_CASTLING_PENALTY;

    return score;
}

template<bool IsEndgame>
inline int64_t Evaluator::evalKingActivitySide(const chess::Board& b, int side) noexcept {
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return 0;

    const int sign = (side == 0) ? 1 : -1;
    const int ksq = __builtin_ctzll(kingBB);
    const uint64_t proximityMask = KING_PROXIMITY_MASKS[ksq];

    if constexpr (IsEndgame) {
        const uint64_t friends =
            b.pawns_bb[side]   |
            b.knights_bb[side] |
            b.bishops_bb[side] |
            b.rooks_bb[side]   |
            b.queens_bb[side];
        const int friendsNearKing = __builtin_popcountll(friends & proximityMask);
        return sign * friendsNearKing * engine::KING_ACTIVITY_BONUS;
    }

    const int opp = side ^ 1;
    const uint64_t enemies =
        b.pawns_bb[opp]   |
        b.knights_bb[opp] |
        b.bishops_bb[opp] |
        b.rooks_bb[opp]   |
        b.queens_bb[opp];
    const int enemiesNearKing = __builtin_popcountll(enemies & proximityMask);
    return sign * enemiesNearKing * engine::KING_SAFETY_PENALTY;
}

int64_t Evaluator::evalKingActivity(const chess::Board& b, bool isEndgame) noexcept {
    if (isEndgame) {
        return evalKingActivitySide<true>(b, 0) + evalKingActivitySide<true>(b, 1);
    }
    return evalKingActivitySide<false>(b, 0) + evalKingActivitySide<false>(b, 1);
}

template<int Side>
inline int64_t Evaluator::evalEndgameKingActivitySide(const chess::Board& b) noexcept {
    static constexpr int CENTER[4] = {27, 28, 35, 36};
    const uint64_t kbb = b.kings_bb[Side];
    if (!kbb) [[unlikely]] return 0;

    const int sign = (Side == 0) ? 1 : -1;
    const int sq = __builtin_ctzll(kbb);
    int best = manhattan(sq, CENTER[0]);
    best = std::min(best, manhattan(sq, CENTER[1]));
    best = std::min(best, manhattan(sq, CENTER[2]));
    best = std::min(best, manhattan(sq, CENTER[3]));
    return sign * (7 - best) * 10;
}

int64_t Evaluator::evalEndgameKingActivity(const chess::Board& b) noexcept {
    int64_t scoreWhite = evalEndgameKingActivitySide<0>(b);
    int64_t scoreBlack = evalEndgameKingActivitySide<1>(b);

    return scoreBlack + scoreWhite;
}

} // namespace engine
