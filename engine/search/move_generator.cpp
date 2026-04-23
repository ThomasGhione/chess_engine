#include "move_generator.hpp"

#include "../inl/bitboard_helpers.inl"
#include "sorter.hpp"

namespace engine {

namespace {

__attribute__((always_inline))
inline void appendMoveByIndex(MoveList<chess::Board::Move>& moves, uint8_t from, uint8_t to) noexcept {
    moves.emplace_back();
    chess::Board::Move& m = moves[moves.size - 1];
    m.from.index = from;
    m.to.index = to;
    m.promotionPiece = '\0';
}

__attribute__((always_inline))
inline void appendPromotionSetByIndex(MoveList<chess::Board::Move>& moves, uint8_t from, uint8_t to) noexcept {
    moves.emplace_back();
    chess::Board::Move& q = moves[moves.size - 1];
    q.from.index = from;
    q.to.index = to;
    q.promotionPiece = 'q';

    moves.emplace_back();
    chess::Board::Move& r = moves[moves.size - 1];
    r.from.index = from;
    r.to.index = to;
    r.promotionPiece = 'r';

    moves.emplace_back();
    chess::Board::Move& b = moves[moves.size - 1];
    b.from.index = from;
    b.to.index = to;
    b.promotionPiece = 'b';

    moves.emplace_back();
    chess::Board::Move& n = moves[moves.size - 1];
    n.from.index = from;
    n.to.index = to;
    n.promotionPiece = 'n';
}

constexpr uint64_t computeBetweenExclusiveConstexpr(uint8_t from, uint8_t to) noexcept {
    if (from == to) return 0ULL;

    const int fromFile = chess::file(from);
    const int fromRank = chess::rank(from);
    const int toFile = chess::file(to);
    const int toRank = chess::rank(to);
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

    uint64_t mask = 0ULL;
    int f = fromFile + stepFile;
    int r = fromRank + stepRank;
    while (f != toFile || r != toRank) {
        const uint8_t sq = static_cast<uint8_t>((r << 3) | f);
        mask |= chess::Board::bitMask(sq);
        f += stepFile;
        r += stepRank;
    }

    return mask & ~chess::Board::bitMask(to);
}

inline constexpr std::array<std::array<uint64_t, 64>, 64> BETWEEN_EXCLUSIVE_LUT = [] {
    std::array<std::array<uint64_t, 64>, 64> lut{};
    for (uint8_t from = 0; from < 64; ++from) {
        for (uint8_t to = 0; to < 64; ++to) {
            lut[from][to] = computeBetweenExclusiveConstexpr(from, to);
        }
    }
    return lut;
}();

template<bool InCheck, uint8_t PieceType>
__attribute__((always_inline))
inline void appendNonPawnTacticalNoChecks(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint64_t pieceBB,
    uint64_t occ,
    uint64_t oppOcc,
    uint64_t pinnedMask,
    const uint64_t pinRayBySquare[64],
    uint64_t evasionMask,
    uint8_t pieceCode) noexcept {
    while (pieceBB) {
        const uint8_t from = engine::popLSB(pieceBB);
        const uint64_t fromBit = chess::Board::bitMask(from);

        uint64_t attacks = pieces::generateMovesByType<PieceType>(from, occ) & oppOcc;
        if constexpr (InCheck) {
            attacks &= evasionMask;
        }
        if (pinnedMask & fromBit) {
            attacks &= pinRayBySquare[from];
        }

        if constexpr (InCheck) {
            while (attacks) {
                const uint8_t to = engine::popLSB(attacks);
                if (b.isLegalPseudoMove(from, to, pieceCode, true)) {
                    appendMoveByIndex(moves, from, to);
                }
            }
        } else {
            while (attacks) {
                appendMoveByIndex(moves, from, engine::popLSB(attacks));
            }
        }
    }
}

template<bool InCheck>
__attribute__((always_inline))
inline void appendPawnTacticalNoChecks(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint64_t pawnBB,
    int side,
    bool isWhite,
    uint64_t occ,
    uint64_t oppOcc,
    uint64_t enPassantBit,
    uint8_t enPassantIndex,
    uint64_t pinnedMask,
    const uint64_t pinRayBySquare[64],
    uint64_t evasionMask,
    uint8_t pawnPiece) noexcept {
    const uint8_t promotionRank = chess::Board::promotionRank(isWhite);
    const uint8_t prePromotionRank = isWhite ? 1 : 6;
    const int pawnPushDelta = isWhite ? -8 : 8;

    while (pawnBB) {
        const uint8_t from = engine::popLSB(pawnBB);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const uint64_t pawnAttacks = pieces::PAWN_ATTACKS[side][from];
        const uint64_t epCandidate = pawnAttacks & enPassantBit;
        uint64_t attacks = (pawnAttacks & oppOcc) | epCandidate;

        if (chess::Board::rank(from) == prePromotionRank) {
            const uint8_t frontSq = static_cast<uint8_t>(static_cast<int>(from) + pawnPushDelta);
            attacks |= chess::Board::bitMask(frontSq) & ~occ;
        }

        if constexpr (InCheck) {
            attacks &= evasionMask;
        }
        if (pinnedMask & fromBit) {
            attacks &= pinRayBySquare[from];
        }
        attacks |= epCandidate;

        while (attacks) {
            const uint8_t to = engine::popLSB(attacks);
            if constexpr (InCheck) {
                if (!b.isLegalPseudoMove(from, to, pawnPiece, true)) {
                    continue;
                }
            } else {
                const bool isEnPassant = (epCandidate != 0ULL) && (to == enPassantIndex);
                if (isEnPassant && !b.isLegalPseudoMove(from, to, pawnPiece)) {
                    continue;
                }
            }

            if (chess::Board::rank(to) == promotionRank) {
                appendPromotionSetByIndex(moves, from, to);
            } else {
                appendMoveByIndex(moves, from, to);
            }
        }
    }
}

} // namespace

MoveList<chess::Board::Move> MoveGenerator::generateLegalMoves(const chess::Board& b, bool inCheckKnown, 
                                                               bool inCheckValue, bool inDoubleCheckValue) noexcept {
    // Macro-step 1: Initialize side-to-move context and occupancy masks.
    MoveList<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const int side = chess::Board::colorToIndex(color);
    const bool isWhite = (color == chess::Board::WHITE);
    
    //FIXME: modificare Board per non dover avere queste variabili qui
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t pawns   = b.pawns_bb[side];
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
    const bool inCheck = inCheckKnown ? inCheckValue : b.inCheck(color);
    const bool inDoubleCheck = inCheck
        ? (inCheckKnown ? inDoubleCheckValue : b.isDoubleCheck(color))
        : false;
    const bool singleCheck = inCheck && !inDoubleCheck;
    const uint8_t pawnPiece = chess::Board::PAWN | color;
    const uint8_t knightPiece = chess::Board::KNIGHT | color;
    const uint8_t bishopPiece = chess::Board::BISHOP | color;
    const uint8_t rookPiece = chess::Board::ROOK | color;
    const uint8_t queenPiece = chess::Board::QUEEN | color;
    const uint8_t kingPiece = chess::Board::KING | color;

    // Macro-step 2: Compute check-evasion mask when in single-check.
    uint64_t evasionMask = ~0ULL;
    if (singleCheck) {
        computeCheckEvasionMasks(b, color, inCheck, inDoubleCheck, evasionMask);
    }

    //FIXME: Mettere precodizione per eliminare codizione
    // Macro-step 3: Generate king moves and castling moves first.
    // ================= KING =================
    if (!kings) [[unlikely]] return moves; // No king found, return empty move list

    //FIXME: Rendere codice tra "---" una funzione helper AKA: generateKingMoves
    //---
    const uint8_t from = __builtin_ctzll(kings);
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
        const uint8_t f = chess::Board::file(from);
        if (f <= 5 && b.isLegalPseudoMove(from, from + 2, kingPiece)) {
            const uint8_t castleTo = from + 2;
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{castleTo}});
        }
        if (f >= 2 && b.isLegalPseudoMove(from, from - 2, kingPiece)) {
            const uint8_t castleTo = from - 2;
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{castleTo}});
        }
    }
    //---

    // In double-check only king moves are legal.
    if (inDoubleCheck) return moves;

    // Macro-step 4: Compute pin rays to restrict non-king piece mobility.
    // NOTE: for performance, legality checks are skipped for many non-king moves
    // when check/pin filters already guarantee king safety.
    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare;
    //FIXME: Rendere piu' leggibile codizione. Creare funzione helper
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays(b, fromC, isWhite, pinnedMask, pinRayBySquare.data());
    }

    // Macro-step 5: Generate all non-king moves applying check/pin filtering.
    while (pawns) {
        const uint8_t from = engine::popLSB(pawns);
        const uint64_t fromBit = chess::Board::bitMask(from);
        uint64_t mask = pieces::getPawnForwardPushes(from, isWhite, occ);
        const uint64_t epCandidate = (pieces::PAWN_ATTACKS[side][from] & enPassantBit) ? enPassantBit : 0ULL;
        mask |= (pieces::PAWN_ATTACKS[side][from] & oppOcc) | epCandidate;
        if (singleCheck) mask &= evasionMask;
        if (pinnedMask & fromBit) mask &= pinRayBySquare[from];
        // Keep EP candidate for legality check because EP changes occupancy on two squares.
        mask |= epCandidate;
        addPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, pawnPiece, enPassant, hasEnPassant);
    }    
    
    generateNonPawnLegalMoves<[](uint8_t sq, uint64_t) { return pieces::KNIGHT_ATTACKS[sq]; }>(
        b, moves, knights, occ, ownOcc, singleCheck, evasionMask, pinnedMask, pinRayBySquare.data(), inCheck, inDoubleCheck, knightPiece);
    generateNonPawnLegalMoves<pieces::getBishopAttacks>(
        b, moves, bishops, occ, ownOcc, singleCheck, evasionMask, pinnedMask, pinRayBySquare.data(), inCheck, inDoubleCheck, bishopPiece);
    generateNonPawnLegalMoves<pieces::getRookAttacks>(
        b, moves, rooks, occ, ownOcc, singleCheck, evasionMask, pinnedMask, pinRayBySquare.data(), inCheck, inDoubleCheck, rookPiece);
    generateNonPawnLegalMoves<pieces::getQueenAttacks>(
        b, moves, queens, occ, ownOcc, singleCheck, evasionMask, pinnedMask, pinRayBySquare.data(), inCheck, inDoubleCheck, queenPiece);

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
MoveList<chess::Board::Move> MoveGenerator::generateTacticalMoves(const chess::Board& b, bool includeChecks, 
                                                                  bool inCheckKnown, bool inCheckValue, 
                                                                  bool inDoubleCheckValue) noexcept {
    MoveList<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const int side = chess::Board::colorToIndex(color);
    const bool isWhite = (color == chess::Board::WHITE);

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

    const bool inCheck = inCheckKnown ? inCheckValue : b.inCheck(color);
    const bool inDoubleCheck = inCheck
        ? (inCheckKnown ? inDoubleCheckValue : b.isDoubleCheck(color))
        : false;

    const uint8_t pawnPiece = chess::Board::PAWN | color;
    const uint8_t knightPiece = chess::Board::KNIGHT | color;
    const uint8_t bishopPiece = chess::Board::BISHOP | color;
    const uint8_t rookPiece = chess::Board::ROOK | color;
    const uint8_t queenPiece = chess::Board::QUEEN | color;
    const uint8_t kingPiece = chess::Board::KING | color;

    if (!kings) [[unlikely]] return moves;
    const uint8_t kingIndex = __builtin_ctzll(kings);

    if (inDoubleCheck) {
        uint64_t attacks = pieces::KING_ATTACKS[kingIndex] & oppOcc;
        if (!includeChecks) {
            while (attacks) {
                appendMoveByIndex(moves, kingIndex, engine::popLSB(attacks));
            }
        } else {
            addTacticalMovesFromMask(b, attacks, kingIndex, kingPiece, false, isWhite, true,
                                     enPassant, hasEnPassant, moves);
        }
        return moves;
    }

    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare{};
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays(b, chess::Coords{kingIndex}, isWhite, pinnedMask, pinRayBySquare.data());
    }

    if (!includeChecks) {
        const uint8_t enPassantIndex = hasEnPassant ? enPassant.index : 0;

        if (!inCheck) {
            appendPawnTacticalNoChecks<false>(
                b, moves, pawns, side, isWhite, occ, oppOcc, enPassantBit, enPassantIndex,
                pinnedMask, pinRayBySquare.data(), 0ULL, pawnPiece);
            appendNonPawnTacticalNoChecks<false, chess::Board::KNIGHT>(
                b, moves, knights, occ, oppOcc, pinnedMask, pinRayBySquare.data(), 0ULL, knightPiece);
            appendNonPawnTacticalNoChecks<false, chess::Board::BISHOP>(
                b, moves, bishops, occ, oppOcc, pinnedMask, pinRayBySquare.data(), 0ULL, bishopPiece);
            appendNonPawnTacticalNoChecks<false, chess::Board::ROOK>(
                b, moves, rooks, occ, oppOcc, pinnedMask, pinRayBySquare.data(), 0ULL, rookPiece);
            appendNonPawnTacticalNoChecks<false, chess::Board::QUEEN>(
                b, moves, queens, occ, oppOcc, pinnedMask, pinRayBySquare.data(), 0ULL, queenPiece);
        } else {
            uint64_t evasionMask = ~0ULL;
            computeCheckEvasionMasks(b, color, true, false, evasionMask);
            appendPawnTacticalNoChecks<true>(
                b, moves, pawns, side, isWhite, occ, oppOcc, enPassantBit, enPassantIndex,
                pinnedMask, pinRayBySquare.data(), evasionMask, pawnPiece);
            appendNonPawnTacticalNoChecks<true, chess::Board::KNIGHT>(
                b, moves, knights, occ, oppOcc, pinnedMask, pinRayBySquare.data(), evasionMask, knightPiece);
            appendNonPawnTacticalNoChecks<true, chess::Board::BISHOP>(
                b, moves, bishops, occ, oppOcc, pinnedMask, pinRayBySquare.data(), evasionMask, bishopPiece);
            appendNonPawnTacticalNoChecks<true, chess::Board::ROOK>(
                b, moves, rooks, occ, oppOcc, pinnedMask, pinRayBySquare.data(), evasionMask, rookPiece);
            appendNonPawnTacticalNoChecks<true, chess::Board::QUEEN>(
                b, moves, queens, occ, oppOcc, pinnedMask, pinRayBySquare.data(), evasionMask, queenPiece);
        }

        uint64_t kingAttacks = pieces::KING_ATTACKS[kingIndex] & oppOcc;
        while (kingAttacks) {
            appendMoveByIndex(moves, kingIndex, engine::popLSB(kingAttacks));
        }
        return moves;
    }

    if (!inCheck) {
        uint64_t bb = pawns;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            const uint64_t pawnAttacks = pieces::PAWN_ATTACKS[side][from];
            const uint64_t epCandidate = pawnAttacks & enPassantBit;
            uint64_t attacks = (pawnAttacks & oppOcc) | epCandidate;

            const uint8_t rank = chess::Board::rank(from);
            const uint8_t prePromotionRank = isWhite ? 1 : 6;
            if (rank == prePromotionRank) {
                const uint8_t frontSq = static_cast<uint8_t>(static_cast<int>(from) + (isWhite ? -8 : 8));
                attacks |= chess::Board::bitMask(frontSq) & ~occ;
            }
            if (isPinned) attacks &= pinRayBySquare[from];
            attacks |= epCandidate;

            addTacticalMovesFromMask(b, attacks, from, pawnPiece, true, isWhite, true,
                                     enPassant, hasEnPassant, moves);
        }

        generateNonPawnTacticalMoves<[](uint8_t sq, uint64_t) { return pieces::KNIGHT_ATTACKS[sq]; }>(
            b, moves, knights, occ, oppOcc, ~0ULL, pinnedMask, pinRayBySquare.data(), knightPiece, isWhite, enPassant, hasEnPassant);
        generateNonPawnTacticalMoves<pieces::getBishopAttacks>(
            b, moves, bishops, occ, oppOcc, ~0ULL, pinnedMask, pinRayBySquare.data(), bishopPiece, isWhite, enPassant, hasEnPassant);
        generateNonPawnTacticalMoves<pieces::getRookAttacks>(
            b, moves, rooks, occ, oppOcc, ~0ULL, pinnedMask, pinRayBySquare.data(), rookPiece, isWhite, enPassant, hasEnPassant);
        generateNonPawnTacticalMoves<pieces::getQueenAttacks>(
            b, moves, queens, occ, oppOcc, ~0ULL, pinnedMask, pinRayBySquare.data(), queenPiece, isWhite, enPassant, hasEnPassant);
    } else {
        uint64_t evasionMask = ~0ULL;
        computeCheckEvasionMasks(b, color, true, false, evasionMask);

        uint64_t bb = pawns;
        while (bb) {
            const uint8_t from = engine::popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            const uint64_t pawnAttacks = pieces::PAWN_ATTACKS[side][from];
            const uint64_t epCandidate = pawnAttacks & enPassantBit;
            uint64_t attacks = (pawnAttacks & oppOcc) | epCandidate;

            const uint8_t rank = chess::Board::rank(from);
            const uint8_t prePromotionRank = isWhite ? 1 : 6;
            if (rank == prePromotionRank) {
                const uint8_t frontSq = static_cast<uint8_t>(static_cast<int>(from) + (isWhite ? -8 : 8));
                attacks |= chess::Board::bitMask(frontSq) & ~occ;
            }
            attacks &= evasionMask;
            if (isPinned) attacks &= pinRayBySquare[from];
            attacks |= epCandidate;

            addTacticalMovesFromMask(b, attacks, from, pawnPiece, true, isWhite, true,
                                     enPassant, hasEnPassant, moves);
        }

        generateNonPawnTacticalMoves<[](uint8_t sq, uint64_t) { return pieces::KNIGHT_ATTACKS[sq]; }>(
            b, moves, knights, occ, oppOcc, evasionMask, pinnedMask, pinRayBySquare.data(), knightPiece, isWhite, enPassant, hasEnPassant);
        generateNonPawnTacticalMoves<pieces::getBishopAttacks>(
            b, moves, bishops, occ, oppOcc, evasionMask, pinnedMask, pinRayBySquare.data(), bishopPiece, isWhite, enPassant, hasEnPassant);
        generateNonPawnTacticalMoves<pieces::getRookAttacks>(
            b, moves, rooks, occ, oppOcc, evasionMask, pinnedMask, pinRayBySquare.data(), rookPiece, isWhite, enPassant, hasEnPassant);
        generateNonPawnTacticalMoves<pieces::getQueenAttacks>(
            b, moves, queens, occ, oppOcc, evasionMask, pinnedMask, pinRayBySquare.data(), queenPiece, isWhite, enPassant, hasEnPassant);
    }

    uint64_t kingAttacks = pieces::KING_ATTACKS[kingIndex] & oppOcc;
    addTacticalMovesFromMask(b, kingAttacks, kingIndex, kingPiece, false, isWhite, true,
                             enPassant, hasEnPassant, moves);
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
    //FIXME: Rendere chiamata senza true e false che non significano nulla. 
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
    // Macro-step 1: Expand one promotion square into 4 promotion piece choices.
    appendPromotionSetByIndex(moves, fromC.index, toC.index);
}

// ============================================================================
// addPawnMovesFromMask
// ============================================================================
void MoveGenerator::addPawnMovesFromMask(const chess::Board& b, MoveList<chess::Board::Move>& moves, 
                                         uint8_t from, uint64_t mask, bool inCheck, bool inDoubleCheck, 
                                         uint8_t pawnPiece, chess::Coords enPassant, bool hasEnPassant) noexcept {
    //FIXME: Creare pre codizione
    // Macro-step 1: Guard empty mask and precompute pawn metadata.
    if (!mask) [[unlikely]] return;

    const chess::Coords fromC{from};
    const uint8_t fromFile = chess::Board::file(from);
    const bool isWhite = (b.getColor(from) == chess::Board::WHITE);
    const uint8_t promotionRank = chess::Board::promotionRank(isWhite);

    // Macro-step 2: Iterate destinations and enforce EP legality checks.
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        const chess::Coords toC{to};
        const bool isEnPassant = hasEnPassant
            && (toC == enPassant)
            && (chess::Board::file(to) != fromFile);

        // Always check legality for en passant (changes occupancy), otherwise it's already filtered
        if (isEnPassant && !b.isLegalPseudoMove(from, to, pawnPiece, inCheck, inDoubleCheck)) {
            continue;
        }
	
        // Macro-step 3: Emit promotion set or regular pawn move.
        if (chess::Board::rank(to) == promotionRank) {
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
    //FIXME: Mettere precodizione
    // Macro-step 1: Guard empty candidate mask.
    if (!mask) [[unlikely]] return;

    // Macro-step 2: Emit only pseudo-legal moves that pass legality filtering.
    const chess::Coords fromC{from};
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        if (b.isLegalPseudoMove(from, to, piece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }
}

// ============================================================================
// addTacticalMovesFromMask
// ============================================================================
void MoveGenerator::addTacticalMovesFromMask(const chess::Board& b, uint64_t mask, uint8_t from, 
                                             uint8_t piece, bool isPawn, bool isWhite, bool includeChecks, 
                                             chess::Coords enPassant, bool hasEnPassant, 
                                             MoveList<chess::Board::Move>& moves) noexcept {
    // Macro-step 1: Initialize context needed for tactical filtering.
    const chess::Coords fromC{from};
    const uint8_t defaultOppColor = chess::Board::WHITE;
    const uint8_t oppColor = includeChecks
        ? chess::Board::oppositeColor(b.getActiveColor())
        : defaultOppColor;
    const uint8_t fromFile = chess::Board::file(from);

    // Macro-step 2: Iterate candidates and identify tactical categories.
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

	//FIXME: Rendere resto del codice in funzione helper
        const uint8_t toPiece = b.get(to);
        const bool isEnPassant = isPawn && hasEnPassant && (to == enPassant.index)
            && (chess::Board::file(to) != fromFile) && (toPiece == chess::Board::EMPTY);
        const bool isCapture = (toPiece != chess::Board::EMPTY) || isEnPassant;
        const bool isPromotion = isPawn && (chess::Board::rank(to) == chess::Board::promotionRank(isWhite));

        // Fast path: when checks are disabled, skip non-tactical quiet moves immediately.
        if (!includeChecks && !isCapture && !isPromotion) {
            continue;
        }

        // Check legality for en passant and promotions
            if ((isEnPassant || isPromotion) && !b.isLegalPseudoMove(from, to, piece)) {
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
void MoveGenerator::addTacticalMovesFromMaskInCheck(const chess::Board& b, uint64_t mask, 
                                                    uint8_t from, uint8_t piece, bool isPawn, 
                                                    bool isWhite, MoveList<chess::Board::Move>& moves) noexcept {
    // Macro-step 1: Iterate in-check candidates (all legal evasions are tactical).
    const chess::Coords fromC{from};

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        // In check evasion: all legal moves are tactical
        if (!b.isLegalPseudoMove(from, to, piece, true)) {
            continue;
        }

        const chess::Coords toC{to};
        const bool isPromotion = isPawn && (chess::Board::rank(to) == chess::Board::promotionRank(isWhite));
        // Macro-step 2: Emit promotion expansions or plain evasions.
        if (isPromotion) {
            addPromotionMoves(moves, fromC, toC);
            continue;
        }

        moves.emplace_back(chess::Board::Move{fromC, toC});
    }
}

uint64_t MoveGenerator::betweenMaskExclusive(uint8_t from, uint8_t to) noexcept {
    return BETWEEN_EXCLUSIVE_LUT[from][to];
}

// ============================================================================
// computePinRays
// ============================================================================
// Returns a mask with bits for pieces pinned to the king (pinnedMask)
// and an array that stores the pin-ray mask for each square (pinRayBySquare).
void MoveGenerator::computePinRays(const chess::Board& b, chess::Coords kingPos, bool isWhite, 
                                   uint64_t& pinnedMask, uint64_t pinRays[64]) noexcept {
    pinnedMask = 0ULL;
    const uint8_t ownColor = isWhite ? chess::Board::WHITE : chess::Board::BLACK;
    const int us = chess::Board::colorToIndex(ownColor);
    const int them = us ^ 1;
    const uint8_t kingSq = kingPos.index;

    const uint64_t ownOcc = b.pawns_bb[us]
        | b.knights_bb[us]
        | b.bishops_bb[us]
        | b.rooks_bb[us]
        | b.queens_bb[us];
    if (!ownOcc) {
        return;
    }

    const uint64_t rookLikeEnemy = b.rooks_bb[them] | b.queens_bb[them];
    const uint64_t bishopLikeEnemy = b.bishops_bb[them] | b.queens_bb[them];
    if ((rookLikeEnemy | bishopLikeEnemy) == 0ULL) {
        return;
    }

    const uint64_t occWithoutOwn = b.getPiecesBitMap() & ~ownOcc;
    uint64_t rookPinners = pieces::getRookAttacks(kingSq, occWithoutOwn) & rookLikeEnemy;
    uint64_t bishopPinners = pieces::getBishopAttacks(kingSq, occWithoutOwn) & bishopLikeEnemy;

    while (rookPinners) {
        const uint8_t pinnerSq = engine::popLSB(rookPinners);
        const uint64_t between = BETWEEN_EXCLUSIVE_LUT[kingSq][pinnerSq];
        const uint64_t blockers = between & ownOcc;
        if (blockers && ((blockers & (blockers - 1)) == 0ULL)) {
            const uint8_t pinnedSq = static_cast<uint8_t>(__builtin_ctzll(blockers));
            pinnedMask |= blockers;
            pinRays[pinnedSq] = between | chess::Board::bitMask(pinnerSq);
        }
    }

    while (bishopPinners) {
        const uint8_t pinnerSq = engine::popLSB(bishopPinners);
        const uint64_t between = BETWEEN_EXCLUSIVE_LUT[kingSq][pinnerSq];
        const uint64_t blockers = between & ownOcc;
        if (blockers && ((blockers & (blockers - 1)) == 0ULL)) {
            const uint8_t pinnedSq = static_cast<uint8_t>(__builtin_ctzll(blockers));
            pinnedMask |= blockers;
            pinRays[pinnedSq] = between | chess::Board::bitMask(pinnerSq);
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
    
    //FIXME: Fare pre codizione
    const int us = chess::Board::colorToIndex(color);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        evasionMask = 0ULL;
        return;
    }
  
    //FIXME: Creare funzione helper per restituire direttamente checkersMask
    // Macro-step 2: Build checker mask from all enemy piece classes.
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t checkersMask = 0ULL;
    checkersMask |= pieces::PAWN_ATTACKERS_TO[them][kingSq] & b.pawns_bb[them];
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

    const uint8_t checkerSq = __builtin_ctzll(checkersMask);
    const uint8_t checkerType = b.get(checkerSq) & chess::Board::MASK_PIECE_TYPE;

    evasionMask = chess::Board::bitMask(checkerSq);
    //FIXME: Creare funzione inline helper per codizone dentro if.
    if (checkerType == chess::Board::ROOK
        || checkerType == chess::Board::BISHOP
        || checkerType == chess::Board::QUEEN) {
        evasionMask |= betweenMaskExclusive(kingSq, checkerSq);
    }
}



template<uint64_t (*GetAttacks)(uint8_t, uint64_t)>
void MoveGenerator::generateNonPawnLegalMoves(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint64_t bb, uint64_t occ, uint64_t ownOcc,
    bool singleCheck, uint64_t evasionMask,
    uint64_t pinnedMask, const uint64_t pinRayBySquare[64],
    bool inCheck, bool inDoubleCheck, uint8_t pt) noexcept {
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        uint64_t mask = GetAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (pinnedMask & chess::Board::bitMask(from)) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, pt);
    }
}

template<uint64_t (*GetAttacks)(uint8_t, uint64_t)>
void MoveGenerator::generateNonPawnTacticalMoves(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint64_t bb, uint64_t occ, uint64_t oppOcc, uint64_t evasionMask,
    uint64_t pinnedMask, const uint64_t pinRayBySquare[64],
    uint8_t piece, bool isWhite, chess::Coords enPassant, bool hasEnPassant) noexcept {
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        uint64_t attacks = GetAttacks(from, occ) & oppOcc;
        attacks &= evasionMask;
        if (pinnedMask & chess::Board::bitMask(from)) attacks &= pinRayBySquare[from];
        addTacticalMovesFromMask(b, attacks, from, piece, false, isWhite, true,
                                 enPassant, hasEnPassant, moves);
    }
}
} // namespace engine
