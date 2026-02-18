#include "../engine.hpp"
#include "../tt.hpp"

namespace engine {

void Engine::addTacticalMovesFromMask(const chess::Board& b,
                                      MoveList<chess::Board::Move>& moves,
                                      uint8_t from,
                                      uint64_t mask,
                                      bool isPawn,
                                      bool isWhiteToMove,
                                      bool includeChecks,
                                      const chess::Coords& enPassant,
                                      bool inCheck,
                                      bool inDoubleCheck) const noexcept {
    const chess::Coords fromC{from};

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        const uint8_t toPiece = b.get(to);
        const chess::Coords toC{to};
        const bool isEnPassant = isPawn
            && chess::Coords::isInBounds(enPassant)
            && (toC == enPassant)
            && (toPiece == chess::Board::EMPTY);
        const bool isCapture = (toPiece != chess::Board::EMPTY) || isEnPassant;
        const bool isPromotion = isPawn && (toC.rank() == chess::Board::promotionRank(isWhiteToMove));

        // Fast path: when checks are disabled, skip non-tactical quiet moves immediately.
        if (!includeChecks && !isCapture && !isPromotion) {
            continue;
        }

        if (!b.isLegalPseudoMove(from, to, inCheck, inDoubleCheck)) {
            continue;
        }

        if (isPromotion) {
            // Keep all legal underpromotions for tactical accuracy in qsearch.
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
            continue;
        }

        bool shouldAdd = isCapture;

        const uint8_t moverColor = b.getActiveColor();
        const uint8_t oppColor = chess::Board::oppositeColor(moverColor);
        if (!shouldAdd && includeChecks) {
            chess::Board::MoveState tmpState;
            const auto checkMove = chess::Board::Move{fromC, toC, '\0'};
            const_cast<chess::Board&>(b).doMove(checkMove, tmpState, '\0');
            if (const_cast<chess::Board&>(b).inCheck(oppColor)) {
                shouldAdd = true;
            }
            const_cast<chess::Board&>(b).undoMove(checkMove, tmpState);
        }

        if (shouldAdd) {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}

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
    const uint64_t oppOcc = occ & ~ownOcc;
    const chess::Coords enPassant = b.getEnPassant();
    const bool inCheck = b.inCheck(color);
    const bool inDoubleCheck = inCheck && b.isDoubleCheck(color);
    const uint8_t promotionRank = chess::Board::promotionRank(isWhite);

    // ================= KING =================
    if (!kings) [[unlikely]] return moves; // No king found, return empty move list
    

    const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(kings));
    const chess::Coords fromC{from};

    // King moves MUST always check legality (can't move to attacked squares)
    uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;
    while (mask) {
        const uint8_t to = popLSB(mask);
        if (b.isLegalPseudoMove(from, to, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }

    // Castling: illegal when in check.
    if (!inCheck) {
        const uint8_t f = from & 7;
        if (f <= 5 && b.canMoveToBB(fromC, chess::Coords{uint8_t(from + 2)}, inCheck))
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{uint8_t(from + 2)}});
        if (f >= 2 && b.canMoveToBB(fromC, chess::Coords{uint8_t(from - 2)}, inCheck))
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{uint8_t(from - 2)}});
    }

    // In double-check only king moves are legal.
    if (inDoubleCheck) {
        return moves;
    }
    

    // NOTE: All generated moves call canMoveToBB to verify legality
    // This ensures no moves that leave the king in check are generated
    // (e.g., moves that violate pins or double-check responses)
    
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getPawnForwardPushes(from, isWhite, occ);
        uint64_t caps = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
        if (chess::Coords::isInBounds(enPassant)) {
            const uint64_t epMask = chess::Board::bitMask(enPassant.index);
            if (pieces::PAWN_ATTACKS[isWhite][from] & epMask) {
                caps |= epMask;
            }
        }
        mask |= caps;
        addPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck, promotionRank);
    }

    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck);
    }

    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getBishopAttacks(from, occ) & ~ownOcc;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck);
    }

    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getRookAttacks(from, occ) & ~ownOcc;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck);
    }

    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t mask = pieces::getQueenAttacks(from, occ) & ~ownOcc;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck);
    }

    return moves;
}

void Engine::addNonPawnMovesFromMaskFast(const chess::Board& b,
                                         MoveList<chess::Board::Move>& moves,
                                         uint8_t from,
                                         uint64_t mask,
                                         bool inCheck,
                                         bool inDoubleCheck) const noexcept {
    if (!mask) [[unlikely]] return;
    const chess::Coords fromC{from};
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        if (b.isLegalPseudoMove(from, to, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }
}

void Engine::addPawnMovesFromMaskFast(const chess::Board& b,
                                      MoveList<chess::Board::Move>& moves,
                                      uint8_t from,
                                      uint64_t mask,
                                      bool inCheck,
                                      bool inDoubleCheck,
                                      uint8_t promotionRank) const noexcept {
    if (!mask) [[unlikely]] return;
    const chess::Coords fromC{from};
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        const chess::Coords toC{to};
        if (!b.isLegalPseudoMove(from, to, inCheck, inDoubleCheck)) {
            continue;
        }
        if (chess::Board::rankOf(to) == promotionRank) {
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
        } else {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
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
MoveList<chess::Board::Move> Engine::generateTacticalMoves(const chess::Board& b, bool includeChecks,
                                                           bool inCheckKnown, bool inCheckValue,
                                                           bool inDoubleCheckValue) const noexcept {
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
    const chess::Coords enPassant = b.getEnPassant();
    
    const bool inCheck = inCheckKnown ? inCheckValue : b.inCheck(color);
    const bool inDoubleCheck = inCheck
        ? (inCheckKnown ? inDoubleCheckValue : b.isDoubleCheck(color))
        : false;

    // In double-check only king moves are legal; tactical generator only needs king captures.
    if (inDoubleCheck) {
        if (kings) {
            const uint8_t from = __builtin_ctzll(kings);
            uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
            addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks, enPassant, inCheck, inDoubleCheck);
        }
        return moves;
    }

    // ================= PAWNS (captures and promotions) =================
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        
        // Pawn attacks (captures only)
        uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
        if (chess::Coords::isInBounds(enPassant)) {
            const uint64_t epMask = chess::Board::bitMask(enPassant.index);
            if (pieces::PAWN_ATTACKS[isWhite][from] & epMask) {
                attacks |= epMask;
            }
        }
        
        // Pawn forward pushes from the rank immediately before promotion
        const uint8_t rank = from / 8;
        const uint8_t prePromotionRank = isWhite ? 1 : 6;
        if (rank == prePromotionRank) {
            // Check forward push for promotion
            const int direction = isWhite ? -8 : 8;
            const int frontSq = static_cast<int>(from) + direction;
            if (frontSq >= 0 && frontSq < 64 && !(occ & chess::Board::bitMask(static_cast<uint8_t>(frontSq)))) {
                attacks |= chess::Board::bitMask(static_cast<uint8_t>(frontSq));
            }
        }
        
        addTacticalMovesFromMask(b, moves, from, attacks, true, isWhite, includeChecks, enPassant, inCheck, inDoubleCheck);
    }

    // ================= KNIGHTS (captures only) =================
    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::KNIGHT_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks, enPassant, inCheck, inDoubleCheck);
    }

    // ================= BISHOPS (captures only) =================
    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getBishopAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks, enPassant, inCheck, inDoubleCheck);
    }

    // ================= ROOKS (captures only) =================
    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getRookAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks, enPassant, inCheck, inDoubleCheck);
    }

    // ================= QUEENS (captures only) =================
    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        uint64_t attacks = pieces::getQueenAttacks(from, occ) & oppOcc;
        addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks, enPassant, inCheck, inDoubleCheck);
    }

    // ================= KING (captures only) =================
    if (kings) {
        const uint8_t from = __builtin_ctzll(kings); // King: no need for poplsb (only one)
        uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks, enPassant, inCheck, inDoubleCheck);
    }

    return moves;
}

} // namespace engine
