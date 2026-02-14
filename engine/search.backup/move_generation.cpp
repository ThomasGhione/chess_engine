#include "../engine.hpp"
#include "../tt.hpp"

namespace engine {

MoveList<chess::Board::Move>
Engine::generateLegalMoves(const chess::Board& b) const noexcept {
    MoveList<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const int side = chess::Board::colorToIndex(color);
    const bool isWhite = (side == 0);

    const uint64_t occ = b.getPiecesBitMap();

    const uint64_t pawns   = b.pawns_bb[side];
    const uint64_t knights = b.knights_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    const uint64_t rooks   = b.rooks_bb[side];
    const uint64_t queens  = b.queens_bb[side];
    const uint64_t kings   = b.kings_bb[side];

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;
    const bool inCheck = b.inCheck(color);

    // ================= KING =================
    if (!kings) [[unlikely]] return moves; // No king found, return empty move list
    

    const uint8_t from = popLSB(const_cast<uint64_t&>(kings));
    const chess::Coords fromC{from};

    // King moves MUST always check legality (can't move to attacked squares)
    uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;
    while (mask) {
        const uint8_t to = popLSB(mask);
        if (b.canMoveToBB(fromC, chess::Coords{to}, inCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }

    // Castling: always needs legality check
    const uint8_t f = from & 7;
    if (f <= 5 && b.canMoveToBB(fromC, chess::Coords{uint8_t(from + 2)}, inCheck))
        moves.emplace_back(chess::Board::Move{fromC, chess::Coords{uint8_t(from + 2)}});
    if (f >= 2 && b.canMoveToBB(fromC, chess::Coords{uint8_t(from - 2)}, inCheck))
        moves.emplace_back(chess::Board::Move{fromC, chess::Coords{uint8_t(from - 2)}});
    

    // NOTE: All generated moves call canMoveToBB to verify legality
    // This ensures no moves that leave the king in check are generated
    // (e.g., moves that violate pins or double-check responses)
    
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::PAWN_ATTACKS[isWhite][from] | pieces::getPawnForwardPushes(from, isWhite, occ);
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getBishopAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getRookAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getQueenAttacks(from, occ) & ~ownOcc;
        addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
    }

    return moves;
}

// ============================================================================
// GENERATE TACTICAL MOVES - Helper for quiescence search
// ============================================================================
// Generates only moves that are tactically relevant:
// 1. Captures (including en passant)
// 2. Pawn promotions (even non-capturing)
// 3. Checks (optional, controlled by QSEARCH_INCLUDE_CHECKS (TODO))
//
// This is a simplified version of generateLegalMoves() optimized for qsearch
MoveList<chess::Board::Move> Engine::generateTacticalMoves(const chess::Board& b, bool includeChecks) const noexcept {
    MoveList<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const int side = chess::Board::colorToIndex(color);
    const bool isWhite = (side == 0);

    const uint64_t occ = b.getPiecesBitMap();

    const uint64_t pawns   = b.pawns_bb[side];
    const uint64_t knights = b.knights_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    const uint64_t rooks   = b.rooks_bb[side];
    const uint64_t queens  = b.queens_bb[side];
    const uint64_t kings   = b.kings_bb[side];

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;
    
    // Opponent occupancy: all pieces minus our pieces
    const uint64_t oppOcc = occ & ~ownOcc;
    
    const bool inCheck = b.inCheck(color);

    // Helper to add tactical moves (captures, promotions, and optionally checks)
    auto addTacticalMovesFromMask = [&](uint8_t from, uint64_t mask, bool isPawn) {
        const chess::Coords fromC{from};
        
        while (mask) {
            const uint8_t to = __builtin_ctzll(mask);
            mask &= (mask - 1);
            
            const chess::Coords toC{to};
            const bool isCapture = (b.get(toC) != chess::Board::EMPTY);
            const bool isPromotion = isPawn && (toC.rank() == chess::Board::promotionRank(isWhite));
            
            // Only add if it's a capture, promotion, or (if includeChecks) a check
            bool shouldAdd = (isCapture || isPromotion);
            
            if (!shouldAdd && includeChecks) {
                // Check if this move gives check (expensive doMove/undoMove)
                if (b.canMoveToBB(fromC, toC, inCheck)) {
                    chess::Board::MoveState tmpState;
                    const_cast<chess::Board&>(b).doMove({fromC, toC, '\0'}, tmpState, isPawn && isPromotion ? 'q' : '\0');
                    if (const_cast<chess::Board&>(b).inCheck(isWhite ? chess::Board::BLACK : chess::Board::WHITE)) {
                        shouldAdd = true;
                    }
                    const_cast<chess::Board&>(b).undoMove({fromC, toC, '\0'}, tmpState);
                }
            }
            
            if (shouldAdd && b.canMoveToBB(fromC, toC, inCheck)) {
                moves.emplace_back(chess::Board::Move{fromC, toC});
            }
        }
    };

    // ================= PAWNS (captures and promotions) =================
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        
        // Pawn attacks (captures only)
        uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
        
        // Pawn forward pushes (only if promotion rank)
        const uint8_t rank = from / 8;
        const uint8_t promotionRank = isWhite ? 6 : 1; // Rank 7 for white, rank 2 for black (0-indexed)
        if (rank == promotionRank) {
            // Check forward push for promotion
            const int direction = isWhite ? 8 : -8;
            const uint8_t frontSq = from + direction;
            if (!(occ & chess::Board::bitMask(frontSq))) {
                attacks |= chess::Board::bitMask(frontSq);
            }
        }
        
        addTacticalMovesFromMask(from, attacks, true);
    }

    // ================= KNIGHTS (captures only) =================
    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::KNIGHT_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= BISHOPS (captures only) =================
    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getBishopAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= ROOKS (captures only) =================
    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getRookAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= QUEENS (captures only) =================
    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getQueenAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    // ================= KING (captures only) =================
    if (kings) {
        const uint8_t from = __builtin_ctzll(kings); // King: no need for poplsb (only one)
        uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(from, attacks, false);
    }

    return moves;
}

} // namespace engine
