#include "evaluator.hpp"
#include "../piecevaluetables.hpp"
#include <algorithm>
#include <cstring>
namespace engine {

int64_t Evaluator::evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept {
    // Cheap pawn-shield evaluation. Avoid undefined shifts around edges.
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const uint64_t kingBB = b.kings_bb[side];
        if (!kingBB) [[unlikely]] continue;
        const int sq = __builtin_ctzll(kingBB);
        const int sign = (side == 0) ? 1 : -1;

        // Verify castling status: King in castling position AND castling rights lost
        // (If rights still exist, we haven't castled yet!)
        const bool rightsLost = !b.getCastle(side == 0 ? 0 : 2) && !b.getCastle(side == 0 ? 1 : 3);
        const bool onCastlingSquare = (side == 0) ? (sq == 62 || sq == 58) : (sq == 6 || sq == 2);
        const bool hasCastled = onCastlingSquare && rightsLost;
        
        if (!hasCastled) {
            score += sign * (-KING_NON_CASTLING_PENALTY);
            
            // This weakens king safety if castling rights are still available
            const bool canCastleKingside = (side == 0) ? b.getCastle(0) : b.getCastle(2);
            const bool canCastleQueenside = (side == 0) ? b.getCastle(1) : b.getCastle(3);
            
            if (side == 0) { // WHITE
                // Kingside castling pawns: f2(53), g2(54), h2(55)
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(53) | chess::Board::bitMask(54) | chess::Board::bitMask(55);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~whitePawns);
                    score -= movedPawns * 6; // penalità per pedone mosso
                }
                
                // Queenside castling pawns: b2(49), c2(50), d2(51)
                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(49) | chess::Board::bitMask(50) | chess::Board::bitMask(51);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~whitePawns);
                    score -= movedPawns * 6; // penalità per pedone mosso
                }
            } else { // BLACK
                // Kingside castling pawns: f7(13), g7(14), h7(15)
                if (canCastleKingside) {
                    constexpr uint64_t KINGSIDE_PAWNS_START = chess::Board::bitMask(13) | chess::Board::bitMask(14) | chess::Board::bitMask(15);
                    const int movedPawns = __builtin_popcountll(KINGSIDE_PAWNS_START & ~blackPawns);
                    score += movedPawns * 6; // bonus per WHITE (black indebolito)
                }
                
                // Queenside castling pawns: b7(9), c7(10), d7(11)
                if (canCastleQueenside) {
                    constexpr uint64_t QUEENSIDE_PAWNS_START = chess::Board::bitMask(9) | chess::Board::bitMask(10) | chess::Board::bitMask(11);
                    const int movedPawns = __builtin_popcountll(QUEENSIDE_PAWNS_START & ~blackPawns);
                    score += movedPawns * 6; // bonus per WHITE (black indebolito)
                }
            }
        }
        
        uint64_t shieldSquares = 0ULL;
        if (side == 0) {
            // White king: pawns in front are towards lower indices (south)
            if (sq >= 8) shieldSquares |= chess::Board::bitMask(sq - 8);
            if (sq >= 7 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq - 7);
            if (sq >= 9 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq - 9);
            score += sign * __builtin_popcountll(whitePawns & shieldSquares) * CASTLE_PAWN_SUPPORT_BONUS;
        } else {
            // Black king: pawns in front are towards higher indices (north)
            if (sq <= 55) shieldSquares |= chess::Board::bitMask(sq + 8);
            if (sq <= 56 && (sq & 7) != 0) shieldSquares |= chess::Board::bitMask(sq + 7);
            if (sq <= 54 && (sq & 7) != 7) shieldSquares |= chess::Board::bitMask(sq + 9);
            score += sign * __builtin_popcountll(blackPawns & shieldSquares) * CASTLE_PAWN_SUPPORT_BONUS;
        }
    }

    return score;
}

int64_t Evaluator::evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept {
    int64_t score = 0;

    for (int side = 0; side < 2; ++side) {
        const int oppSide = side ^ 1;
        const int sign = (side == 0) ? 1 : -1;
        
        // Get enemy king position
        const uint64_t enemyKingBB = b.kings_bb[oppSide];
        if (!enemyKingBB) [[unlikely]] continue;
        
        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        
        // King zone: king square + all adjacent squares (up to 8)
        const uint64_t kingZone = pieces::KING_ATTACKS[enemyKingSq] | chess::Board::bitMask(enemyKingSq);
        
        constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL; // rank 1 & 2 (bit 56-63 + 48-55)
        constexpr uint64_t BLACK_MINOR_START = 0x000000000000FFFFULL; // rank 8 & 7 (bit 0-7 + 8-15)
        
        const uint64_t developedKnights = (side == 0) 
            ? (b.knights_bb[side] & ~WHITE_MINOR_START)
            : (b.knights_bb[side] & ~BLACK_MINOR_START);
        const uint64_t developedBishops = (side == 0)
            ? (b.bishops_bb[side] & ~WHITE_MINOR_START)
            : (b.bishops_bb[side] & ~BLACK_MINOR_START);
        
        // Count attackers and accumulate weighted attack value
        int attackerCount = 0;
        int64_t attackWeight = 0;
        
        // Knights attacking king zone (only if developed)
        if (developedKnights && (data[side].knightAttacks & kingZone)) {
            attackerCount++;
            attackWeight += KING_ATTACK_WEIGHT_KNIGHT;
        }
        
        // Bishops attacking king zone (only if developed)
        if (developedBishops && (data[side].bishopAttacks & kingZone)) {
            attackerCount++;
            attackWeight += KING_ATTACK_WEIGHT_BISHOP;
        }
        
        // Rooks attacking king zone (rooks are naturally less developed early, so allow them)
        if (data[side].rookAttacks & kingZone) {
            attackerCount++;
            attackWeight += KING_ATTACK_WEIGHT_ROOK;
        }
        
        // Queen attacking king zone (allow, but queen early is penalized elsewhere)
        if (data[side].queenAttacks & kingZone) {
            attackerCount++;
            attackWeight += KING_ATTACK_WEIGHT_QUEEN;
        }
        
        // Non-linear scaling: the more attackers, the more dangerous
        // REDUCED: Now requires 2+ attackers AND reduces the scale factor
        // 2 attackers: weight * 2/6 = weight/3 (moderate bonus)
        // 3 attackers: weight * 3/6 = weight/2 (significant)
        // 4 attackers: weight * 4/6 = 2*weight/3 (devastating)
        if (attackerCount >= 2) {
            // Only give significant bonus when 2+ piece types attack the king zone
            // REDUCED: Divided by 6 instead of 4 to be less aggressive
            score += sign * (attackWeight * attackerCount / 6);
        }
    }

    return score;
}

int64_t Evaluator::evalKingActivity(const chess::Board& b, bool isEndgame) noexcept {
    int64_t score = 0;

    // White side
    {
        const uint64_t kingBB = b.kings_bb[0];
        if (kingBB) [[likely]] {
            const int ksq = __builtin_ctzll(kingBB);
            const uint64_t proximityMask = KING_PROXIMITY_MASKS[ksq];

            if (isEndgame) {
                const uint64_t friends =
                    b.pawns_bb[0]   |
                    b.knights_bb[0] |
                    b.bishops_bb[0] |
                    b.rooks_bb[0]   |
                    b.queens_bb[0];
                const int friendsNearKing = __builtin_popcountll(friends & proximityMask);
                score += friendsNearKing * KING_ACTIVITY_BONUS;
            } else {
                const uint64_t enemies =
                    b.pawns_bb[1]   |
                    b.knights_bb[1] |
                    b.bishops_bb[1] |
                    b.rooks_bb[1]   |
                    b.queens_bb[1];
                const int enemiesNearKing = __builtin_popcountll(enemies & proximityMask);
                score += enemiesNearKing * KING_SAFETY_PENALTY;
            }
        }
    }

    // Black side
    {
        const uint64_t kingBB = b.kings_bb[1];
        if (kingBB) [[likely]] {
            const int ksq = __builtin_ctzll(kingBB);
            const uint64_t proximityMask = KING_PROXIMITY_MASKS[ksq];

            if (isEndgame) {
                const uint64_t friends =
                    b.pawns_bb[1]   |
                    b.knights_bb[1] |
                    b.bishops_bb[1] |
                    b.rooks_bb[1]   |
                    b.queens_bb[1];
                const int friendsNearKing = __builtin_popcountll(friends & proximityMask);
                score -= friendsNearKing * KING_ACTIVITY_BONUS;
            } else {
                const uint64_t enemies =
                    b.pawns_bb[0]   |
                    b.knights_bb[0] |
                    b.bishops_bb[0] |
                    b.rooks_bb[0]   |
                    b.queens_bb[0];
                const int enemiesNearKing = __builtin_popcountll(enemies & proximityMask);
                score -= enemiesNearKing * KING_SAFETY_PENALTY;
            }
        }
    }

    return score;
}

int64_t Evaluator::evalEndgameKingActivity(const chess::Board& b) noexcept {
    static constexpr int CENTER[4] = {27, 28, 35, 36}; // d4 e4 d5 e5
    int64_t score = 0;

    // White side
    {
        const uint64_t kbb = b.kings_bb[0];
        if (kbb) [[likely]] {
            const int sq = __builtin_ctzll(kbb);
            int best = manhattan(sq, CENTER[0]);
            best = std::min(best, manhattan(sq, CENTER[1]));
            best = std::min(best, manhattan(sq, CENTER[2]));
            best = std::min(best, manhattan(sq, CENTER[3]));
            score += (7 - best) * 10;
        }
    }

    // Black side
    {
        const uint64_t kbb = b.kings_bb[1];
        if (kbb) [[likely]] {
            const int sq = __builtin_ctzll(kbb);
            int best = manhattan(sq, CENTER[0]);
            best = std::min(best, manhattan(sq, CENTER[1]));
            best = std::min(best, manhattan(sq, CENTER[2]));
            best = std::min(best, manhattan(sq, CENTER[3]));
            score -= (7 - best) * 10;
        }
    }
    
    return score;
}

int64_t Evaluator::evalCastlingBonus(const chess::Board& b) noexcept {
    // Castling positions (a8=0, h1=63):
    // White: g1=62 (kingside), c1=58 (queenside), f1=61, d1=59
    // Black: g8=6 (kingside), c8=2 (queenside), f8=5, d8=3
    static constexpr uint64_t WHITE_KING_CASTLED  = (chess::Board::bitMask(62) | chess::Board::bitMask(58));
    static constexpr uint64_t WHITE_ROOK_CASTLED  = (chess::Board::bitMask(61) | chess::Board::bitMask(59));
    static constexpr uint64_t BLACK_KING_CASTLED  = (chess::Board::bitMask(6)  | chess::Board::bitMask(2));
    static constexpr uint64_t BLACK_ROOK_CASTLED  = (chess::Board::bitMask(5)  | chess::Board::bitMask(3));
    static constexpr int64_t LOSS_OF_CASTLING_PENALTY = 60;

    int64_t score = 0;

    // WHITE
    const bool whiteHasCastled = (b.kings_bb[0] & WHITE_KING_CASTLED) && (b.rooks_bb[0] & WHITE_ROOK_CASTLED);
    const bool whiteCanCastle = b.getCastle(0) || b.getCastle(1); // kingside or queenside
    
    score += whiteHasCastled * CASTLING_BONUS;
    score -= (!whiteHasCastled && !whiteCanCastle) * LOSS_OF_CASTLING_PENALTY;

    // BLACK
    const bool blackHasCastled = (b.kings_bb[1] & BLACK_KING_CASTLED) && (b.rooks_bb[1] & BLACK_ROOK_CASTLED);
    const bool blackCanCastle = b.getCastle(2) || b.getCastle(3); // kingside or queenside
    
    score -= blackHasCastled * CASTLING_BONUS;
    score += (!blackHasCastled && !blackCanCastle) * LOSS_OF_CASTLING_PENALTY;

    return score;
}

} // namespace engine
