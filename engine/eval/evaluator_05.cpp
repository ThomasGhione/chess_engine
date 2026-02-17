#include "evaluator.hpp"
#include "../piecevaluetables.hpp"
#include <algorithm>
#include <cstring>
namespace engine {

void Evaluator::computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
    std::memset(data, 0, 2 * sizeof(AttackData));

    for (int side = 0; side < 2; ++side) {
        AttackData& d = data[side];
        
        const uint64_t ownOcc = (side == 0) 
            ? (b.pawns_bb[0] | b.knights_bb[0] | b.bishops_bb[0] | b.rooks_bb[0] | b.queens_bb[0] | b.kings_bb[0])
            : (b.pawns_bb[1] | b.knights_bb[1] | b.bishops_bb[1] | b.rooks_bb[1] | b.queens_bb[1] | b.kings_bb[1]);
        
        // Pawns - usa lookup table (no magic bitboards)
        uint64_t pawns = b.pawns_bb[side];
        // pieces::PAWN_ATTACKS is indexed by isWhite bool (0=black, 1=white),
        // while side is 0=white, 1=black.
        const bool isWhite = (side == 0);
        while (pawns) {
            const int sq = popLSB(pawns);
            d.pawnAttacks |= pieces::PAWN_ATTACKS[isWhite][sq];
        }
        d.allAttacks = d.pawnAttacks;

        // Knights - usa lookup table (no magic bitboards)
        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = popLSB(knights);
            const uint64_t attacks = pieces::KNIGHT_ATTACKS[sq];
            d.knightAttacks |= attacks;
            // CORRETTO: mobility = caselle dove il pezzo può muoversi LEGALMENTE
            // = (case attaccate) MENO (case occupate da pezzi tuoi)
            // Includes captures (enemy pieces) as legal moves
            const int mobility = __builtin_popcountll(attacks & ~ownOcc);
            d.knightMobility += mobility;
        }
        d.allAttacks |= d.knightAttacks;

        // Bishops - magic bitboards necessari
        uint64_t bishops = b.bishops_bb[side];
        while (bishops) {
            const int sq = popLSB(bishops);
            const uint64_t attacks = pieces::getBishopAttacks(sq, occ);
            d.bishopAttacks |= attacks;
            d.bishopMobility += __builtin_popcountll(attacks & ~ownOcc);
        }
        d.allAttacks |= d.bishopAttacks;

        // Rooks - magic bitboards necessari
        uint64_t rooks = b.rooks_bb[side];
        while (rooks) {
            const int sq = popLSB(rooks);
            const uint64_t attacks = pieces::getRookAttacks(sq, occ);
            d.rookAttacks |= attacks;
            d.rookMobility += __builtin_popcountll(attacks & ~ownOcc);
        }
        d.allAttacks |= d.rookAttacks;

        // Queens - magic bitboards necessari
        uint64_t queens = b.queens_bb[side];
        while (queens) {
            const int sq = popLSB(queens);
            const uint64_t attacks = pieces::getQueenAttacks(sq, occ);
            d.queenAttacks |= attacks;
            d.queenMobility += __builtin_popcountll(attacks & ~ownOcc);
        }
        d.allAttacks |= d.queenAttacks;
        
        // Mark as computed
        d.isComputed = true;
    }
}

int64_t Evaluator::evalBlockedPawnByBishops(const chess::Board& b) noexcept {
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
                // base penalty magnitude (positive value)
                int penaltyVal = 30; // base (reduced from 40)
                // central files (d=3, e=4) are worse
                if (file == 3 || file == 4) penaltyVal += 25;
                // extra penalty if pawn still on starting rank (white:6, black:1)
                const int startRank = (side == 0) ? 6 : 1;
                if (rank == startRank) penaltyVal += 20;

                // Apply penalty (negative value) properly signed
                score += sign * (-penaltyVal);
            }
        }
    }

    return score;
}

int64_t Evaluator::evalRookEndgamePressure(const chess::Board& b) noexcept {
    int64_t score = 0;

    // Detect if we're in a favorable Rook endgame (R+K vs K or better)
    const int whiteRooks = __builtin_popcountll(b.rooks_bb[0]);
    const int blackRooks = __builtin_popcountll(b.rooks_bb[1]);
    
    const bool whiteHasRookAdvantage = (whiteRooks > blackRooks);
    const bool blackHasRookAdvantage = (blackRooks > whiteRooks);
    
    if (!whiteHasRookAdvantage && !blackHasRookAdvantage) {
        return 0;
    }

    // For each side with Rook advantage, bonus enemy king when it approaches edges
    for (int side = 0; side < 2; ++side) {
        const bool sideHasAdvantage = (side == 0) ? whiteHasRookAdvantage : blackHasRookAdvantage;
        if (!sideHasAdvantage) continue;

        const int oppSide = side ^ 1;
        const int oppQueens = __builtin_popcountll(b.queens_bb[oppSide]);
        const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
        const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
        const int oppRooks2 = (side == 0) ? blackRooks : whiteRooks;
        const int oppMaterial = oppQueens * 900 + oppRooks2 * 500 + oppBishops * 330 + oppKnights * 320;
        
        // Only apply if opponent has at most a minor piece (no queen, no rook)
        if (oppMaterial > 400) continue;

        const int sign = (side == 0) ? 1 : -1;
        const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
        if (!enemyKingBB) continue;

        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const int rank = chess::Board::rankOf(enemyKingSq);
        const int file = chess::Board::fileOf(enemyKingSq);

        // Distance to edge (minimum of rank distance to rank 0/7 and file distance to file 0/7)
        const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});

        // Bonus: closer to edge = better (enemy king trapped!)
        // Formula: 7 - distToEdge = 0 (center) to 7 (edge)
        const int edgeProximity = 7 - distToEdge;
        
        // Number of our Rooks involved in the attack
        const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
        
        // Delegate 2+ rooks case to evalDoubleRookEndgame to avoid double counting
        if (ourRooks >= 2)
            continue; 

        // Scale the edge bonus based on number of rooks:
        // - 1 Rook: base bonus (ROOK_EG_EDGE_BONUS)
        const int64_t edgeBonus = engine::ROOK_EG_EDGE_BONUS;
        
        // Gradually increase bonus as enemy king approaches edge
        // 0 (center) -> 7 (edge)
        score += sign * edgeProximity * edgeBonus;

        // Additional bonus for King+Rook coordination: distance between our King and enemy King
        const uint64_t ourKingBB = b.kings_bb[side];
        if (ourKingBB) {
            const int ourKingSq = __builtin_ctzll(ourKingBB);
            const int kingDist = manhattan(ourKingSq, enemyKingSq);
            
            // For single rook, king support is essential.
            // Base: 14 - distance (bonus increases as king gets closer)
            const int proximityBonus = std::max(0, 14 - kingDist);
            score += sign * proximityBonus * engine::ROOK_EG_PRESSURE_BONUS / 14;
        }
    }

    return score;
}

int64_t Evaluator::evalQueenEndgamePressure(const chess::Board& b) noexcept {
    int64_t score = 0;

    const int whiteQueens = __builtin_popcountll(b.queens_bb[0]);
    const int blackQueens = __builtin_popcountll(b.queens_bb[1]);
    
    if (whiteQueens == 0 && blackQueens == 0) {
        return 0; // No queens on board
    }

    for (int side = 0; side < 2; ++side) {
        const int ourQueens = (side == 0) ? whiteQueens : blackQueens;
        
        // Only apply if we have at least 1 queen
        if (ourQueens == 0) continue;
        
        // Calculate opponent's material (excluding king)
        const int oppSide = side ^ 1;
        
        // Simplified material check: Only apply pressure bonus if we have a CLEAR material advantage.
        // If material is roughly equal (e.g. Q vs Q), don't force king to edge (it might be unsafe).
        // Material threshold: We must be ahead by at least 2 pawns (200cp).
        // Using getMaterialDelta would be better, but local estimation is faster here.
        const int oppPawns = __builtin_popcountll(b.pawns_bb[oppSide]);
        const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
        const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
        const int oppRooks = __builtin_popcountll(b.rooks_bb[oppSide]);
        const int oppQueens = (side == 0) ? blackQueens : whiteQueens;

        const int oppMaterial = oppQueens * 900 + oppRooks * 500 + 
                                oppBishops * 330 + oppKnights * 320 + oppPawns * 100;
                            
        // Standard check: Our Queen (900) vs Opponent Material.
        // If Opponent Material > 700 (e.g. R+B), it's not a clear "Mating Net" simplified endgame.
        if (oppMaterial > 700) continue;

        const int sign = (side == 0) ? 1 : -1;
        const uint64_t enemyKingBB = b.kings_bb[oppSide];
        if (!enemyKingBB) continue;

        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const int rank = chess::Board::rankOf(enemyKingSq);
        const int file = chess::Board::fileOf(enemyKingSq);

        // Queen mating: push to edge/corner - STRONG bonus
        const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
        const int edgeProximity = 7 - distToEdge;
        
        // Queen is very strong: HUGE bonus for pushing king to edge
        constexpr int64_t QUEEN_EG_EDGE_BONUS = 120; // Decreased from 200 (too aggressive, caused sacrifices). 
                                                     // Material is dominant.
        score += sign * edgeProximity * QUEEN_EG_EDGE_BONUS;

        // King coordination: CRITICAL for Queen mates
        const uint64_t ourKingBB = b.kings_bb[side];
        if (ourKingBB) {
            const int ourKingSq = __builtin_ctzll(ourKingBB);
            const int kingDist = manhattan(ourKingSq, enemyKingSq);
            
            // King MUST be close for mating
            const int proximityBonus = std::max(0, 14 - kingDist);
            score += sign * proximityBonus * 20; // Decreased from 40
        }
        
        // Additional: Queen proximity to enemy king
        const uint64_t queenBB = b.queens_bb[side];
        if (queenBB) {
            // Find closest queen if multiple
            uint64_t tempQueens = queenBB;
            int bestQueenDist = 100;
            while (tempQueens) {
                 const int qSq = __builtin_ctzll(tempQueens);
                 tempQueens &= tempQueens - 1;
                 bestQueenDist = std::min(bestQueenDist, manhattan(qSq, enemyKingSq));
            }
            
            // Queen should be reasonably close (2-5 squares optimal for control)
            if (bestQueenDist >= 2 && bestQueenDist <= 5) {
                score += sign * 40; // Reduced from 80
            } else if (bestQueenDist <= 7) {
                score += sign * 15; // Reduced from 30
            }
        }
    }

    return score;
}

int64_t Evaluator::evalDoubleRookEndgame(const chess::Board& b) noexcept {
    int64_t score = 0;

    const int whiteRooks = __builtin_popcountll(b.rooks_bb[0]);
    const int blackRooks = __builtin_popcountll(b.rooks_bb[1]);
    
    // Only apply when we have 2+ rooks and opponent has fewer
    for (int side = 0; side < 2; ++side) {
        const int ourRooks = (side == 0) ? whiteRooks : blackRooks;
        const int oppRooks = (side == 0) ? blackRooks : whiteRooks;
        
        if (ourRooks < 2 || ourRooks <= oppRooks) continue;

        const int oppSide = side ^ 1;
        const int oppQueens = __builtin_popcountll(b.queens_bb[oppSide]);
        const int oppBishops = __builtin_popcountll(b.bishops_bb[oppSide]);
        const int oppKnights = __builtin_popcountll(b.knights_bb[oppSide]);
        const int oppMaterial = oppQueens * 900 + oppRooks * 500 + oppBishops * 330 + oppKnights * 320;
        
        // Only apply if opponent material is low enough that double rooks can force mate
        // (opponent has at most a minor piece, no queen)
        if (oppMaterial > 500) continue;

        const int sign = (side == 0) ? 1 : -1;
        const uint64_t enemyKingBB = b.kings_bb[side ^ 1];
        if (!enemyKingBB) continue;

        const int enemyKingSq = __builtin_ctzll(enemyKingBB);
        const int rank = chess::Board::rankOf(enemyKingSq);
        const int file = chess::Board::fileOf(enemyKingSq);

        // Double rook: push to edge (very strong)
        const int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
        const int edgeProximity = 7 - distToEdge;
        
        // With 2 rooks, edge pressure is VERY strong
        constexpr int64_t DOUBLE_ROOK_EDGE_BONUS = 100;
        score += sign * edgeProximity * DOUBLE_ROOK_EDGE_BONUS;

        // Rook coordination: check if rooks are on same rank or file
        uint64_t rooksBB = b.rooks_bb[side];
        if (__builtin_popcountll(rooksBB) >= 2) {
            const int rook1 = __builtin_ctzll(rooksBB);
            rooksBB &= (rooksBB - 1); // Remove first rook
            const int rook2 = __builtin_ctzll(rooksBB);
            
            const int r1_rank = chess::Board::rankOf(rook1);
            const int r1_file = chess::Board::fileOf(rook1);
            const int r2_rank = chess::Board::rankOf(rook2);
            const int r2_file = chess::Board::fileOf(rook2);
            
            // Bonus if rooks are on same rank or file (coordinated)
            if (r1_rank == r2_rank || r1_file == r2_file) {
                score += sign * 50; // Coordination bonus
            }
            
            // Extra bonus if they control the rank/file where enemy king is
            if (r1_rank == rank || r2_rank == rank || r1_file == file || r2_file == file) {
                score += sign * 40; // Controlling enemy king's escape
            }
        }

        // King support (less critical than Queen, but still helpful)
        const uint64_t ourKingBB = b.kings_bb[side];
        if (ourKingBB) {
            const int ourKingSq = __builtin_ctzll(ourKingBB);
            const int kingDist = manhattan(ourKingSq, enemyKingSq);
            
            const int proximityBonus = std::max(0, 14 - kingDist);
            score += sign * proximityBonus * 8; // 8 cp per square (less than Queen)
        }
    }

    return score;
}

} // namespace engine
