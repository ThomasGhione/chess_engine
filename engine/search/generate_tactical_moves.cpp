#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {
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
                                                           bool inDoubleCheckValue) noexcept {
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
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const uint64_t enPassantBit = hasEnPassant ? chess::Board::bitMask(enPassant.index) : 0ULL;
    
    const bool inCheck = inCheckKnown ? inCheckValue : b.inCheck(color);
    const bool inDoubleCheck = inCheck
        ? (inCheckKnown ? inDoubleCheckValue : b.isDoubleCheck(color))
        : false;
    const uint8_t pawnPiece = static_cast<uint8_t>(chess::Board::PAWN | color);
    const uint8_t knightPiece = static_cast<uint8_t>(chess::Board::KNIGHT | color);
    const uint8_t bishopPiece = static_cast<uint8_t>(chess::Board::BISHOP | color);
    const uint8_t rookPiece = static_cast<uint8_t>(chess::Board::ROOK | color);
    const uint8_t queenPiece = static_cast<uint8_t>(chess::Board::QUEEN | color);
    const uint8_t kingPiece = static_cast<uint8_t>(chess::Board::KING | color);

    // In double-check only king moves are legal; tactical generator only needs king captures.
    if (inDoubleCheck) {
        if (kings) {
            const uint8_t from = __builtin_ctzll(kings);
            uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
            addTacticalMovesFromMask(b, attacks, from, kingPiece, false, isWhite, includeChecks,
                                     enPassant, inCheck, inDoubleCheck, moves, false);
        }
        return moves;
    }

    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare{};
    computePinRays(b, color, pinnedMask, pinRayBySquare);

    if (!inCheck) {
        // ================= PAWNS (captures and promotions, no-check fast path) =================
        uint64_t bb = pawns;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            const bool skipLegalityCheck = !isPinned;

            uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
            uint64_t epCandidate = 0ULL;
            if (hasEnPassant && (pieces::PAWN_ATTACKS[isWhite][from] & enPassantBit)) {
                attacks |= enPassantBit;
                epCandidate = enPassantBit;
            }

            const uint8_t rank = from >> 3;
            const uint8_t prePromotionRank = isWhite ? 1 : 6;
            if (rank == prePromotionRank) {
                const int direction = isWhite ? -8 : 8;
                const int frontSq = static_cast<int>(from) + direction;
                if (frontSq >= 0 && frontSq < 64 && !(occ & chess::Board::bitMask(static_cast<uint8_t>(frontSq)))) {
                    attacks |= chess::Board::bitMask(static_cast<uint8_t>(frontSq));
                }
            }
            if (isPinned) attacks &= pinRayBySquare[from];
            if (epCandidate) attacks |= epCandidate;

            addTacticalMovesFromMask(b, attacks, from, pawnPiece, true, isWhite, includeChecks,
                                     enPassant, false, false, moves, skipLegalityCheck);
        }

        bb = knights;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::KNIGHT_ATTACKS[from] & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, knightPiece, false, isWhite, includeChecks,
                                     enPassant, false, false, moves, !isPinned);
        }

        bb = bishops;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getBishopAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, bishopPiece, false, isWhite, includeChecks,
                                     enPassant, false, false, moves, !isPinned);
        }

        bb = rooks;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getRookAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, rookPiece, false, isWhite, includeChecks,
                                     enPassant, false, false, moves, !isPinned);
        }

        bb = queens;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getQueenAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, queenPiece, false, isWhite, includeChecks,
                                     enPassant, false, false, moves, !isPinned);
        }
    } else {
        uint64_t evasionMask = ~0ULL;
        computeCheckEvasionMasks(b, color, true, false, evasionMask);

        const bool useSpecializedInCheckHelper = !includeChecks;

        // ================= PAWNS (captures and promotions, in-check path) =================
        uint64_t bb = pawns;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;

            uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
            uint64_t epCandidate = 0ULL;
            if (hasEnPassant && (pieces::PAWN_ATTACKS[isWhite][from] & enPassantBit)) {
                attacks |= enPassantBit;
                epCandidate = enPassantBit;
            }

            const uint8_t rank = from >> 3;
            const uint8_t prePromotionRank = isWhite ? 1 : 6;
            if (rank == prePromotionRank) {
                const int direction = isWhite ? -8 : 8;
                const int frontSq = static_cast<int>(from) + direction;
                if (frontSq >= 0 && frontSq < 64 && !(occ & chess::Board::bitMask(static_cast<uint8_t>(frontSq)))) {
                    attacks |= chess::Board::bitMask(static_cast<uint8_t>(frontSq));
                }
            }
            attacks &= evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (epCandidate) attacks |= epCandidate;

            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, pawnPiece, true, isWhite, enPassant, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, pawnPiece, true, isWhite, includeChecks,
                                         enPassant, true, false, moves, false);
            }
        }

        bb = knights;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::KNIGHT_ATTACKS[from] & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, knightPiece, false, isWhite, enPassant, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, knightPiece, false, isWhite, includeChecks,
                                         enPassant, true, false, moves, false);
            }
        }

        bb = bishops;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::getBishopAttacks(from, occ) & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, bishopPiece, false, isWhite, enPassant, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, bishopPiece, false, isWhite, includeChecks,
                                         enPassant, true, false, moves, false);
            }
        }

        bb = rooks;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::getRookAttacks(from, occ) & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, rookPiece, false, isWhite, enPassant, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, rookPiece, false, isWhite, includeChecks,
                                         enPassant, true, false, moves, false);
            }
        }

        bb = queens;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::getQueenAttacks(from, occ) & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, queenPiece, false, isWhite, enPassant, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, queenPiece, false, isWhite, includeChecks,
                                         enPassant, true, false, moves, false);
            }
        }
    }

    // ================= KING (captures only) =================
    if (kings) {
        const uint8_t from = __builtin_ctzll(kings); // King: no need for poplsb (only one)
        uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(b, attacks, from, kingPiece, false, isWhite, includeChecks,
                                 enPassant, inCheck, false, moves, false);
    }

    return moves;
}

} // namespace engine
