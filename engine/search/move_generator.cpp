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

template<uint8_t PieceType>
__attribute__((always_inline))
inline void appendNonPawnTacticalNoChecks(
    MoveList<chess::Board::Move>& moves,
    uint64_t pieceBB,
    uint64_t occ,
    uint64_t oppOcc,
    uint64_t pinnedMask,
    const uint64_t pinRayBySquare[64]) noexcept {
    while (pieceBB) {
        const uint8_t from = engine::popLSB(pieceBB);
        const uint64_t fromBit = chess::Board::bitMask(from);

        uint64_t attacks = pieces::generateMovesByType<PieceType>(from, occ) & oppOcc;
        if (pinnedMask & fromBit) {
            attacks &= pinRayBySquare[from];
        }

        while (attacks) {
            appendMoveByIndex(moves, from, engine::popLSB(attacks));
        }
    }
}

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

        if (pinnedMask & fromBit) {
            attacks &= pinRayBySquare[from];
        }
        attacks |= epCandidate;

        while (attacks) {
            const uint8_t to = engine::popLSB(attacks);
            const bool isEnPassant = (epCandidate != 0ULL) && (to == enPassantIndex);

            if (isEnPassant) {
                if (!b.isLegalPseudoMove(from, to, pawnPiece, false)) continue;
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
    const uint8_t pawnPromotionRank = chess::Board::promotionRank(isWhite);
    const uint8_t kingPiece = chess::Board::KING | color;

    // Macro-step 2: Compute check-evasion mask when in single-check.
    uint64_t evasionMask = ~0ULL;
    if (singleCheck) {
        computeCheckEvasionMasks(b, color, evasionMask);
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
    // NOTE: per performance, don't zero-initialize this array.
    // It's only ever read where pinnedMask has a bit set.
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
        addPawnMovesFromMask(
            b, moves, from, mask, inCheck, inDoubleCheck, pawnPiece,
            isWhite, pawnPromotionRank, enPassant, hasEnPassant);
    }    
    
    if (singleCheck) {
        generateNonPawnLegalMoves<true, chess::Board::KNIGHT>(
            moves, knights, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<true, chess::Board::BISHOP>(
            moves, bishops, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<true, chess::Board::ROOK>(
            moves, rooks, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<true, chess::Board::QUEEN>(
            moves, queens, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
    } else {
        generateNonPawnLegalMoves<false, chess::Board::KNIGHT>(
            moves, knights, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<false, chess::Board::BISHOP>(
            moves, bishops, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<false, chess::Board::ROOK>(
            moves, rooks, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<false, chess::Board::QUEEN>(
            moves, queens, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
    }

    return moves;
}

// ============================================================================
// GENERATE TACTICAL MOVES - Helper for quiescence search
// ============================================================================
// Generates only tactical qsearch moves:
// 1. Captures (including en passant)
// 2. Pawn promotions (also non-capturing)
//
// Assumptions fixed by the only call-site (generateQSearchTacticalMoves):
// - includeChecks = false
// - inCheckKnown = true
// - inCheckValue = false
// - inDoubleCheckValue = false
// So this path is specialized for "side to move is NOT in check".
MoveList<chess::Board::Move> MoveGenerator::generateTacticalMoves(const chess::Board& b) noexcept {
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

    const uint8_t pawnPiece = chess::Board::PAWN | color;

    if (!kings) [[unlikely]] return moves;
    const uint8_t kingIndex = __builtin_ctzll(kings);

    uint64_t pinnedMask = 0ULL;
    // NOTE: per performance, don't zero-initialize this array.
    std::array<uint64_t, 64> pinRayBySquare;
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays(b, chess::Coords{kingIndex}, isWhite, pinnedMask, pinRayBySquare.data());
    }

    const uint8_t enPassantIndex = hasEnPassant ? enPassant.index : 0;
    appendPawnTacticalNoChecks(
        b, moves, pawns, side, isWhite, occ, oppOcc, enPassantBit, enPassantIndex,
        pinnedMask, pinRayBySquare.data(), pawnPiece);
    appendNonPawnTacticalNoChecks<chess::Board::KNIGHT>(moves, knights, occ, oppOcc, pinnedMask, pinRayBySquare.data());
    appendNonPawnTacticalNoChecks<chess::Board::BISHOP>(moves, bishops, occ, oppOcc, pinnedMask, pinRayBySquare.data());
    appendNonPawnTacticalNoChecks<chess::Board::ROOK>(moves, rooks, occ, oppOcc, pinnedMask, pinRayBySquare.data());
    appendNonPawnTacticalNoChecks<chess::Board::QUEEN>(moves, queens, occ, oppOcc, pinnedMask, pinRayBySquare.data());

    uint64_t kingAttacks = pieces::KING_ATTACKS[kingIndex] & oppOcc;
    while (kingAttacks) {
        appendMoveByIndex(moves, kingIndex, engine::popLSB(kingAttacks));
    }
    return moves;
}

engine::Sorter::MovePickerData MoveGenerator::generateQSearchEvasions(const chess::Board& b) noexcept {
    // Macro-step 1: Generate full legal evasions from the current board.
    MoveList<chess::Board::Move> evasions = generateLegalMoves(b);

    // Macro-step 2: Fast-return for checkmate/stalemate nodes.
    if (evasions.is_empty()) {
        return engine::Sorter::MovePickerData{};
    }

    // Macro-step 3: Reorder evasions with forcing moves first using Sorter policy.
    MoveList<chess::Board::Move> sorted = engine::Sorter::sortEvasionsForcingFirst(evasions, b);
    
    engine::Sorter::MovePickerData data;
    data.moves = sorted;
    data.size = sorted.size;
    data.currentIndex = 0;
    return data;
}

engine::Sorter::MovePickerData MoveGenerator::generateQSearchTacticalMoves(
    const chess::Board& b,
    int32_t standPat,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool usIsWhite,
    int32_t searchDepth) noexcept {
    // Macro-step 1: Generate tactical candidate moves for qsearch.
    MoveList<chess::Board::Move> tacticalMoves = generateTacticalMoves(b);

    // Macro-step 2: Return early when no tactical continuation exists.
    if (tacticalMoves.is_empty()) {
        return engine::Sorter::MovePickerData{};
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
                                         uint8_t pawnPiece, bool isWhite, uint8_t promotionRank,
                                         chess::Coords enPassant, bool hasEnPassant) noexcept {
    //FIXME: Creare pre codizione
    // Macro-step 1: Guard empty mask and precompute pawn metadata.
    if (!mask) [[unlikely]] return;

    (void)isWhite;
    const chess::Coords fromC{from};
    const uint8_t fromFile = chess::Board::file(from);

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
// computePinRays
// ============================================================================
// Returns a mask with bits for pieces pinned to the king (pinnedMask)
// and an array that stores the pin-ray mask for each square (pinRayBySquare).
void MoveGenerator::computePinRays(const chess::Board& b, chess::Coords kingPos, bool isWhite, 
                                   uint64_t& pinnedMask, uint64_t pinRays[64]) noexcept {
    pinnedMask = 0ULL;
    const int us = isWhite ? 0 : 1;
    const int them = us ^ 1;
    const uint8_t kingSq = kingPos.index;

    const uint64_t ownOcc = b.pawns_bb[us] | b.knights_bb[us] | b.bishops_bb[us] | b.rooks_bb[us] | b.queens_bb[us];
    if (!ownOcc) {
        return;
    }

    const uint64_t rookLikeEnemy = b.rooks_bb[them] | b.queens_bb[them];
    const uint64_t bishopLikeEnemy = b.bishops_bb[them] | b.queens_bb[them];
    if ((rookLikeEnemy | bishopLikeEnemy) == 0ULL) {
        return;
    }

    // Geometric fast-path: if no enemy slider is geometrically aligned with our king
    // (even with an empty board), we don't need to compute real occupancy rays.
    if (!(pieces::getRookAttacks(kingSq, 0ULL) & rookLikeEnemy) &&
        !(pieces::getBishopAttacks(kingSq, 0ULL) & bishopLikeEnemy)) {
        return;
    }

    const uint64_t occWithoutOwn = b.getPiecesBitMap() ^ ownOcc;
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
    uint64_t& evasionMask) noexcept {
    // This helper is called only from single-check paths.
    evasionMask = 0ULL;

    const int us = chess::Board::colorToIndex(color);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        return;
    }
  
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    const uint64_t occ = b.getPiecesBitMap();

    const uint64_t rookCheckers = pieces::getRookAttacks(kingSq, occ) & (b.rooks_bb[them] | b.queens_bb[them]);
    const uint64_t bishopCheckers = pieces::getBishopAttacks(kingSq, occ) & (b.bishops_bb[them] | b.queens_bb[them]);
    const uint64_t checkersMask =
        (pieces::PAWN_ATTACKERS_TO[them][kingSq] & b.pawns_bb[them])
        | (pieces::KNIGHT_ATTACKS[kingSq] & b.knights_bb[them])
        | (pieces::KING_ATTACKS[kingSq] & b.kings_bb[them])
        | rookCheckers
        | bishopCheckers;

    // single-check expected; fallback to king-only evasions on inconsistent data
    if ((checkersMask & (checkersMask - 1)) != 0ULL) {
        return;
    }

    if (!checkersMask) [[unlikely]] {
        evasionMask = ~0ULL;
        return;
    }

    const uint8_t checkerSq = __builtin_ctzll(checkersMask);
    const uint64_t checkerBit = chess::Board::bitMask(checkerSq);
    evasionMask = checkerBit;
    if ((rookCheckers | bishopCheckers) & checkerBit) {
        evasionMask |= BETWEEN_EXCLUSIVE_LUT[kingSq][checkerSq];
    }
}



template<bool InCheck, uint8_t PieceType>
void MoveGenerator::generateNonPawnLegalMoves(
    MoveList<chess::Board::Move>& moves,
    uint64_t bb,
    uint64_t occ,
    uint64_t ownOcc,
    uint64_t evasionMask,
    uint64_t pinnedMask,
    const uint64_t pinRayBySquare[64]) noexcept {
    while (bb) {
        const uint8_t from = engine::popLSB(bb);
        uint64_t mask = pieces::generateMovesByType<PieceType>(from, occ) & ~ownOcc;
        if constexpr (InCheck) {
            mask &= evasionMask;
        }
        if (pinnedMask & chess::Board::bitMask(from)) mask &= pinRayBySquare[from];

        while (mask) {
            appendMoveByIndex(moves, from, engine::popLSB(mask));
        }
    }
}

} // namespace engine
