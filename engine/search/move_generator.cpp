#include "move_generator.hpp"

#include "../inl/bitboard_helpers.inl"
#include "sorter.hpp"

namespace engine::search {

MoveList<chess::Board::Move> MoveGenerator::generateLegalMoves(const chess::Board& b) noexcept {
    // Macro-step 1: Initialize side-to-move context and occupancy masks.
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
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const uint64_t enPassantBit = hasEnPassant ? chess::Board::bitMask(enPassant.index) : 0ULL;
    const bool inCheck = b.inCheck(color);
    const bool inDoubleCheck = inCheck && b.isDoubleCheck(color);
    const bool singleCheck = inCheck && !inDoubleCheck;
    const uint8_t pawnPiece = static_cast<uint8_t>(chess::Board::PAWN | color);
    const uint8_t knightPiece = static_cast<uint8_t>(chess::Board::KNIGHT | color);
    const uint8_t bishopPiece = static_cast<uint8_t>(chess::Board::BISHOP | color);
    const uint8_t rookPiece = static_cast<uint8_t>(chess::Board::ROOK | color);
    const uint8_t queenPiece = static_cast<uint8_t>(chess::Board::QUEEN | color);
    const uint8_t kingPiece = static_cast<uint8_t>(chess::Board::KING | color);

    // Macro-step 2: Compute check-evasion mask when in single-check.
    uint64_t evasionMask = ~0ULL;
    if (singleCheck) {
        computeCheckEvasionMasks(b, color, inCheck, inDoubleCheck, evasionMask);
    }

    // Macro-step 3: Generate king moves and castling moves first.
    // ================= KING =================
    if (!kings) [[unlikely]] return moves; // No king found, return empty move list

    const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(kings));
    const chess::Coords fromC{from};

    // King moves MUST always check legality (can't move to attacked squares)
    uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;
    while (mask) {
        const uint8_t to = engine::popLSB(mask);
        if (b.isLegalPseudoMove(from, to, kingPiece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }

    if (!inCheck) { // castling: illegal when in check.
        const uint8_t f = from & 7;
        if (f <= 5 && b.isLegalPseudoMove(from, static_cast<uint8_t>(from + 2), inCheck))
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{static_cast<uint8_t>(from + 2)}});
        if (f >= 2 && b.isLegalPseudoMove(from, static_cast<uint8_t>(from - 2), inCheck))
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{static_cast<uint8_t>(from - 2)}});
    }

    // In double-check only king moves are legal.
    if (inDoubleCheck) return moves;

    // Macro-step 4: Compute pin rays to restrict non-king piece mobility.
    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare{};
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays(b, fromC, isWhite, pinnedMask, pinRayBySquare.data());
    }

    // NOTE: for performance, legality checks are skipped for many non-king moves
    // when check/pin filters already guarantee king safety.

    // Macro-step 5: Generate all non-king moves applying check/pin filtering.
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getPawnForwardPushes(from, isWhite, occ);
        uint64_t caps = pieces::PAWN_ATTACKS[side][from] & oppOcc;
        uint64_t epCandidate = 0ULL;
        if (hasEnPassant && (pieces::PAWN_ATTACKS[side][from] & enPassantBit)) {
            caps |= enPassantBit;
            epCandidate = enPassantBit;
        }
        mask |= caps;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        if (epCandidate) {
            // Keep EP candidate for legality check because EP changes occupancy on two squares.
            mask |= epCandidate;
        }
        addPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, pawnPiece, enPassant, hasEnPassant);
    }

    bb = knights;
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, knightPiece);
    }

    bb = bishops;
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getBishopAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, bishopPiece);
    }

    bb = rooks;
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getRookAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, rookPiece);
    }

    bb = queens;
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getQueenAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, queenPiece);
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
MoveList<chess::Board::Move> MoveGenerator::generateTacticalMoves(
    const chess::Board& b,
    bool includeChecks,
    bool inCheckKnown,
    bool inCheckValue,
    bool inDoubleCheckValue) noexcept {
    // Macro-step 1: Initialize side state, occupancies and check flags.
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

    // Macro-step 2: Handle double-check fast path (only king captures are relevant).
    // In double-check only king moves are legal; tactical generator only needs king captures.
    if (inDoubleCheck) {
        if (kings) {
            const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(kings));
            uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
            addTacticalMovesFromMask(b, attacks, from, kingPiece, false, isWhite, includeChecks,
                                     enPassant, hasEnPassant, moves);
        }
        return moves;
    }

    // Macro-step 3: Compute pin rays and dispatch in-check/non-check tactical generation.
    // Get king position for pin ray computation
    chess::Coords kingPos{static_cast<uint8_t>(__builtin_ctzll(b.kings_bb[side]))};

    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare{};

    // Compute pin rays for all pieces
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays(b, kingPos, isWhite, pinnedMask, pinRayBySquare.data());
    }

    if (!inCheck) {
        // ================= PAWNS (captures and promotions, no-check fast path) =================
        uint64_t bb = pawns;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;

            uint64_t attacks = pieces::PAWN_ATTACKS[side][from] & oppOcc;
            uint64_t epCandidate = 0ULL;
            if (hasEnPassant && (pieces::PAWN_ATTACKS[side][from] & enPassantBit)) {
                attacks |= enPassantBit;
                epCandidate = enPassantBit;
            }

            const uint8_t rank = static_cast<uint8_t>(from >> 3);
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
                                     enPassant, hasEnPassant, moves);
        }

        bb = knights;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::KNIGHT_ATTACKS[from] & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, knightPiece, false, isWhite, includeChecks,
                                     enPassant, hasEnPassant, moves);
        }

        bb = bishops;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getBishopAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, bishopPiece, false, isWhite, includeChecks,
                                     enPassant, hasEnPassant, moves);
        }

        bb = rooks;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getRookAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, rookPiece, false, isWhite, includeChecks,
                                     enPassant, hasEnPassant, moves);
        }

        bb = queens;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getQueenAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, attacks, from, queenPiece, false, isWhite, includeChecks,
                                     enPassant, hasEnPassant, moves);
        }
    } else {
        uint64_t evasionMask = ~0ULL;
        computeCheckEvasionMasks(b, color, true, false, evasionMask);

        const bool useSpecializedInCheckHelper = !includeChecks;

        // ================= PAWNS (captures and promotions, in-check path) =================
        uint64_t bb = pawns;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;

            uint64_t attacks = pieces::PAWN_ATTACKS[side][from] & oppOcc;
            uint64_t epCandidate = 0ULL;
            if (hasEnPassant && (pieces::PAWN_ATTACKS[side][from] & enPassantBit)) {
                attacks |= enPassantBit;
                epCandidate = enPassantBit;
            }

            const uint8_t rank = static_cast<uint8_t>(from >> 3);
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
                addTacticalMovesFromMaskInCheck(b, attacks, from, pawnPiece, true, isWhite, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, pawnPiece, true, isWhite, includeChecks,
                                         enPassant, hasEnPassant, moves);
            }
        }

        bb = knights;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::KNIGHT_ATTACKS[from] & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, knightPiece, false, isWhite, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, knightPiece, false, isWhite, includeChecks,
                                         enPassant, hasEnPassant, moves);
            }
        }

        bb = bishops;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::getBishopAttacks(from, occ) & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, bishopPiece, false, isWhite, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, bishopPiece, false, isWhite, includeChecks,
                                         enPassant, hasEnPassant, moves);
            }
        }

        bb = rooks;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::getRookAttacks(from, occ) & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, rookPiece, false, isWhite, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, rookPiece, false, isWhite, includeChecks,
                                         enPassant, hasEnPassant, moves);
            }
        }

        bb = queens;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = (pieces::getQueenAttacks(from, occ) & oppOcc) & evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            if (useSpecializedInCheckHelper) {
                addTacticalMovesFromMaskInCheck(b, attacks, from, queenPiece, false, isWhite, moves);
            } else {
                addTacticalMovesFromMask(b, attacks, from, queenPiece, false, isWhite, includeChecks,
                                         enPassant, hasEnPassant, moves);
            }
        }
    }

    // Macro-step 4: Always include king captures in tactical list.
    // ================= KING (captures only) =================
    if (kings) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(kings)); // King: no need for popLSB (only one)
        uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(b, attacks, from, kingPiece, false, isWhite, includeChecks,
                                 enPassant, hasEnPassant, moves);
    }

    return moves;
}

MoveList<chess::Board::Move> MoveGenerator::generateQSearchEvasions(const chess::Board& b) noexcept {
    // Macro-step 1: Generate full legal evasions from the current board.
    MoveList<chess::Board::Move> evasions = generateLegalMoves(b);

    // Macro-step 2: Fast-return for checkmate/stalemate nodes.
    if (evasions.is_empty()) {
        return evasions;
    }

    // Macro-step 3: Reorder evasions with forcing moves first using Sorter policy.
    return engine::Sorter::sortEvasionsForcingFirst(evasions, b);
}

MoveList<chess::Board::Move> MoveGenerator::generateQSearchTacticalMoves(
    const chess::Board& b,
    int32_t standPat,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool usIsWhite,
    int32_t searchDepth) noexcept {
    // Macro-step 1: Generate tactical candidate moves for qsearch.
    MoveList<chess::Board::Move> tacticalMoves = generateTacticalMoves(b, false, true, false, false);

    // Macro-step 2: Return early when no tactical continuation exists.
    if (tacticalMoves.is_empty()) {
        return tacticalMoves;
    }

    // Macro-step 3: Apply qsearch tactical ordering/pruning policy via Sorter.
    return engine::Sorter::sortTacticalMoves(
        tacticalMoves, b, standPat, alpha, beta, ply, usIsWhite, searchDepth);
}

void MoveGenerator::addPromotionMoves(
    MoveList<chess::Board::Move>& moves,
    const chess::Coords& fromC,
    const chess::Coords& toC) noexcept {
    // Macro-step 1: Expand a single promotion square into the 4 legal promotion piece choices.
    moves.emplace_back(fromC, toC, 'q');
    moves.emplace_back(fromC, toC, 'r');
    moves.emplace_back(fromC, toC, 'b');
    moves.emplace_back(fromC, toC, 'n');
}

// ============================================================================
// addPawnMovesFromMask
// ============================================================================
void MoveGenerator::addPawnMovesFromMask(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint8_t from,
    uint64_t mask,
    bool inCheck,
    bool inDoubleCheck,
    uint8_t pawnPiece,
    chess::Coords enPassant,
    bool hasEnPassant) noexcept {
    // Macro-step 1: Guard empty mask and precompute pawn metadata.
    if (!mask) [[unlikely]] return;

    const chess::Coords fromC{from};
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);
    const bool isWhite = (pawnPiece & chess::Board::MASK_COLOR) == chess::Board::WHITE;
    const uint8_t promotionRank = chess::Board::promotionRank(isWhite);

    // Macro-step 2: Iterate destinations and enforce EP legality checks.
    while (mask) {
        const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(mask));
        mask &= (mask - 1);
        const chess::Coords toC{to};
        const bool isEnPassant = hasEnPassant
            && (toC == enPassant)
            && (static_cast<uint8_t>(to & 7) != fromFile);

        // Always check legality for en passant (changes occupancy), otherwise it's already filtered
        if (isEnPassant && !b.isLegalPseudoMove(from, to, pawnPiece, inCheck, inDoubleCheck)) {
            continue;
        }

        // Macro-step 3: Emit promotion set or regular pawn move.
        if (chess::Board::rankOf(to) == promotionRank) {
            addPromotionMoves(moves, fromC, toC);
        } else {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}

// ============================================================================
// addNonPawnMovesFromMask
// ============================================================================
void MoveGenerator::addNonPawnMovesFromMask(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint8_t from,
    uint64_t mask,
    bool inCheck,
    bool inDoubleCheck,
    uint8_t piece) noexcept {
    // Macro-step 1: Guard empty candidate mask.
    if (!mask) [[unlikely]] return;

    // Macro-step 2: Emit only pseudo-legal moves that pass legality filtering.
    const chess::Coords fromC{from};
    while (mask) {
        const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(mask));
        mask &= (mask - 1);
        if (b.isLegalPseudoMove(from, to, piece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }
}

// ============================================================================
// addTacticalMovesFromMask
// ============================================================================
void MoveGenerator::addTacticalMovesFromMask(
    const chess::Board& b,
    uint64_t mask,
    uint8_t from,
    uint8_t piece,
    bool isPawn,
    bool isWhite,
    bool includeChecks,
    chess::Coords enPassant,
    bool hasEnPassant,
    MoveList<chess::Board::Move>& moves) noexcept {
    // Macro-step 1: Initialize context needed for tactical filtering.
    const chess::Coords fromC{from};
    const uint8_t oppColor = includeChecks
        ? chess::Board::oppositeColor(b.getActiveColor())
        : static_cast<uint8_t>(chess::Board::WHITE);
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);

    // Macro-step 2: Iterate candidates and identify tactical categories.
    while (mask) {
        const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(mask));
        mask &= (mask - 1);

        const uint8_t toPiece = b.get(to);
        const bool isEnPassant = isPawn && hasEnPassant && (to == enPassant.index)
            && (static_cast<uint8_t>(to & 7) != fromFile) && (toPiece == chess::Board::EMPTY);
        const bool isCapture = (toPiece != chess::Board::EMPTY) || isEnPassant;
        const bool isPromotion = isPawn && (chess::Board::rankOf(to) == chess::Board::promotionRank(isWhite));

        // Fast path: when checks are disabled, skip non-tactical quiet moves immediately.
        if (!includeChecks && !isCapture && !isPromotion) {
            continue;
        }

        // Check legality for en passant and promotions
        if ((isEnPassant || isPromotion) && !b.isLegalPseudoMove(from, to, piece, false, false)) {
            continue;
        }

        const chess::Coords toC{to};

        if (isPromotion) {
            addPromotionMoves(moves, fromC, toC);
            continue;
        }

        bool shouldAdd = isCapture;

        // Macro-step 3: Optionally include quiet checking moves.
        if (!shouldAdd && includeChecks) {
            chess::Board::MoveState tmpState;
            const auto checkMove = chess::Board::Move{fromC, toC, '\0'};
            const_cast<chess::Board&>(b).doMove(checkMove, tmpState, '\0');
            if (const_cast<chess::Board&>(b).inCheck(oppColor)) {
                shouldAdd = true;
            }
            const_cast<chess::Board&>(b).undoMove(checkMove, tmpState);
        }

        // Macro-step 4: Emit tactical move.
        if (shouldAdd) {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}

// ============================================================================
// addTacticalMovesFromMaskInCheck
// ============================================================================
void MoveGenerator::addTacticalMovesFromMaskInCheck(
    const chess::Board& b,
    uint64_t mask,
    uint8_t from,
    uint8_t piece,
    bool isPawn,
    bool isWhite,
    MoveList<chess::Board::Move>& moves) noexcept {
    // Macro-step 1: Iterate in-check candidates (all legal evasions are tactical).
    const chess::Coords fromC{from};

    while (mask) {
        const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(mask));
        mask &= (mask - 1);

        const bool isPromotion = isPawn && (chess::Board::rankOf(to) == chess::Board::promotionRank(isWhite));

        // In check evasion: all legal moves are tactical

        if (!b.isLegalPseudoMove(from, to, piece, true, false)) {
            continue;
        }

        const chess::Coords toC{to};

        // Macro-step 2: Emit promotion expansions or plain evasions.
        if (isPromotion) {
            addPromotionMoves(moves, fromC, toC);
            continue;
        }

        moves.emplace_back(chess::Board::Move{fromC, toC});
    }
}

uint64_t MoveGenerator::betweenMaskExclusive(uint8_t from, uint8_t to) noexcept {
    // Macro-step 1: Detect invalid geometry and compute stepping direction.
    if (from == to) [[unlikely]] return 0ULL;

    const int fromFile = chess::Board::fileOf(from);
    const int fromRank = chess::Board::rankOf(from);
    const int toFile = chess::Board::fileOf(to);
    const int toRank = chess::Board::rankOf(to);
    const int df = toFile - fromFile;
    const int dr = toRank - fromRank;

    int stepFile = 0;
    int stepRank = 0;
    if (df == 0) {
        stepRank = (dr > 0) ? 1 : -1;
    } else if (dr == 0) {
        stepFile = (df > 0) ? 1 : -1;
    } else if ((df > 0 ? df : -df) == (dr > 0 ? dr : -dr)) {
        stepFile = (df > 0) ? 1 : -1;
        stepRank = (dr > 0) ? 1 : -1;
    } else {
        return 0ULL;
    }

    // Macro-step 2: Accumulate all intermediate squares between endpoints.
    uint64_t mask = 0ULL;
    int f = fromFile + stepFile;
    int r = fromRank + stepRank;
    while (f != toFile || r != toRank) {
        mask |= chess::Board::bitMask(static_cast<uint8_t>((r << 3) | f));
        f += stepFile;
        r += stepRank;
    }

    // Macro-step 3: Exclude destination square from the final mask.
    mask &= ~chess::Board::bitMask(to);
    return mask;
}

// ============================================================================
// computePinRays
// ============================================================================
// Returns a mask with bits for pieces pinned to the king (pinnedMask)
// and an array that stores the pin-ray mask for each square (pinRayBySquare).
void MoveGenerator::computePinRays(
    const chess::Board& b,
    chess::Coords kingPos,
    bool isWhite,
    uint64_t& pinnedMask,
    uint64_t pinRays[64]) noexcept {
    // Macro-step 1: Prepare side metadata and zero outputs.
    pinnedMask = 0ULL;
    const int us = isWhite ? 0 : 1;
    const int them = us ^ 1;

    // Initialize all pin rays to 0
    for (int i = 0; i < 64; ++i) {
        pinRays[i] = 0ULL;
    }

    const uint8_t kingSq = kingPos.index;
    const uint8_t ownColor = isWhite ? chess::Board::WHITE : chess::Board::BLACK;
    const uint64_t rookLikeEnemy = b.rooks_bb[them] | b.queens_bb[them];
    const uint64_t bishopLikeEnemy = b.bishops_bb[them] | b.queens_bb[them];

    if ((rookLikeEnemy | bishopLikeEnemy) == 0ULL) {
        return;
    }

    // Fast bailout: if no enemy slider is even geometrically aligned with the king,
    // no pin can exist and we can skip directional scans entirely.
    if (((pieces::getRookAttacks(kingSq, 0ULL) & rookLikeEnemy) |
         (pieces::getBishopAttacks(kingSq, 0ULL) & bishopLikeEnemy)) == 0ULL) {
        return;
    }

    // Macro-step 2: Scan each line from king to detect own piece + enemy pinner alignment.
    const int kingFile = chess::Board::fileOf(kingSq);
    const int kingRank = chess::Board::rankOf(kingSq);

    static constexpr int DIRS[8][2] = {
        {0, 1}, {0, -1}, {1, 0}, {-1, 0},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (const auto* dir : DIRS) {
        const int df = dir[0];
        const int dr = dir[1];
        const bool orthogonal = (df == 0 || dr == 0);

        int f = kingFile + df;
        int r = kingRank + dr;
        int pinnedSq = -1;

        while (static_cast<unsigned>(f) < 8U && static_cast<unsigned>(r) < 8U) {
            const uint8_t sq = static_cast<uint8_t>((r << 3) | f);
            const uint8_t piece = b.get(sq);

            if (piece == chess::Board::EMPTY) {
                f += df;
                r += dr;
                continue;
            }

            const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;
            if (pieceColor == ownColor) {  // Own piece
                if (pinnedSq >= 0) {
                    break;
                }
                pinnedSq = sq;
                f += df;
                r += dr;
                continue;
            }

            if (pinnedSq >= 0) {
                const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
                const bool isPinner = orthogonal
                    ? (pieceType == chess::Board::ROOK || pieceType == chess::Board::QUEEN)
                    : (pieceType == chess::Board::BISHOP || pieceType == chess::Board::QUEEN);
                if (isPinner) {
                    pinnedMask |= chess::Board::bitMask(static_cast<uint8_t>(pinnedSq));
                    pinRays[static_cast<size_t>(pinnedSq)] =
                        betweenMaskExclusive(kingSq, sq) | chess::Board::bitMask(sq);
                }
            }
            break;
        }
    }
}

// ============================================================================
// computeCheckEvasionMasks
// ============================================================================
// Returns a mask with bits for squares where pieces can move or interpose
// to evade check (evasionMask).
void MoveGenerator::computeCheckEvasionMasks(
    const chess::Board& b,
    uint8_t color,
    bool inCheck,
    bool inDoubleCheck,
    uint64_t& evasionMask) noexcept {
    // Macro-step 1: Initialize mask and early-outs for non-check nodes.
    evasionMask = ~0ULL;

    if (!inCheck) return;

    const int us = chess::Board::colorToIndex(color);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        evasionMask = 0ULL;
        return;
    }

    // Macro-step 2: Build checker mask from all enemy piece classes.
    const uint8_t kingSq = static_cast<uint8_t>(__builtin_ctzll(kingBB));
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t checkersMask = 0ULL;
    checkersMask |= pieces::PAWN_ATTACKERS_TO[us][kingSq] & b.pawns_bb[them];
    checkersMask |= pieces::KNIGHT_ATTACKS[kingSq] & b.knights_bb[them];
    checkersMask |= pieces::KING_ATTACKS[kingSq] & b.kings_bb[them];
    checkersMask |= pieces::getRookAttacks(kingSq, occ) & (b.rooks_bb[them] | b.queens_bb[them]);
    checkersMask |= pieces::getBishopAttacks(kingSq, occ) & (b.bishops_bb[them] | b.queens_bb[them]);

    // Macro-step 3: Derive evasion constraints for double/single checker cases.
    if (inDoubleCheck || ((checkersMask & (checkersMask - 1)) != 0ULL)) {
        evasionMask = 0ULL;
        return;
    }

    if (!checkersMask) [[unlikely]] {
        evasionMask = ~0ULL;
        return;
    }

    const uint8_t checkerSq = static_cast<uint8_t>(__builtin_ctzll(checkersMask));
    const uint8_t checkerType = b.get(checkerSq) & chess::Board::MASK_PIECE_TYPE;

    evasionMask = chess::Board::bitMask(checkerSq);
    if (checkerType == chess::Board::ROOK
        || checkerType == chess::Board::BISHOP
        || checkerType == chess::Board::QUEEN) {
        evasionMask |= betweenMaskExclusive(kingSq, checkerSq);
    }
}

} // namespace engine::search
