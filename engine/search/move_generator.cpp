#include "move_generator.hpp"

#include "../inl/bitboard_helpers.inl"
#include "sorter.hpp"

namespace engine {

namespace {

__attribute__((always_inline))
inline void appendMoveByIndex(MoveList<chess::Board::Move>& moves, int from, int to) noexcept {
    moves.emplace_back();
    auto& m = moves[moves.size - 1];
    m.from.index = from;
    m.to.index = to;
    m.promotionPiece = '\0';
}

__attribute__((always_inline))
inline void appendPromotionSetByIndex(MoveList<chess::Board::Move>& moves, int from, int to) noexcept {
    constexpr char promos[4] = {'q', 'r', 'b', 'n'};
    for (char p : promos) {
        moves.emplace_back();
        auto& m = moves[moves.size - 1];
        m.from.index = from;
        m.to.index   = to;
        m.promotionPiece = p;
    }
}

constexpr uint64_t computeBetweenExclusiveConstexpr(int from, int to) noexcept {
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
    } else if (std::abs(df) == std::abs(dr)) {
        stepFile = (df > 0) ? 1 : -1;
        stepRank = (dr > 0) ? 1 : -1;
    } else {
        return 0ULL;
    }

    uint64_t mask = 0ULL;
    int f = fromFile + stepFile;
    int r = fromRank + stepRank;
    while (f != toFile || r != toRank) {
        const int sq = (r << 3) | f;
        mask |= chess::Board::bitMask(sq);
        f += stepFile;
        r += stepRank;
    }

    return mask & ~chess::Board::bitMask(to);
}

inline constexpr std::array<std::array<uint64_t, 64>, 64> BETWEEN_EXCLUSIVE_LUT = [] {
    std::array<std::array<uint64_t, 64>, 64> lut{};
    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            lut[from][to] = computeBetweenExclusiveConstexpr(from, to);
        }
    }
    return lut;
}();

template<bool HasPins, uint8_t PieceType>
__attribute__((always_inline))
inline void appendNonPawnTacticalNoChecks(
    MoveList<chess::Board::Move>& moves,
    uint64_t pieceBB,
    uint64_t occ,
    uint64_t oppOcc,
    uint64_t pinnedMask,
    const uint64_t pinRayBySquare[64]) noexcept {
    if constexpr (!HasPins) {
        (void)pinnedMask;
        (void)pinRayBySquare;
    }

    while (pieceBB) {
        const int from = engine::popLSB(pieceBB);

        uint64_t attacks = pieces::generateMovesByType<PieceType>(from, occ) & oppOcc;
        if constexpr (HasPins) {
            const uint64_t fromBit = chess::Board::bitMask(from);
            if (pinnedMask & fromBit) {
                attacks &= pinRayBySquare[from];
            }
        }

        while (attacks) {
            appendMoveByIndex(moves, from, engine::popLSB(attacks));
        }
    }
}

template<bool IsWhite>
__attribute__((always_inline))
inline void appendPawnTacticalNoChecks(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint64_t pawnBB,
    uint64_t occ,
    uint64_t oppOcc,
    uint64_t enPassantBit,
    int enPassantIndex,
    uint64_t pinnedMask,
    const uint64_t pinRayBySquare[64],
    uint8_t pawnPiece) noexcept {
    constexpr int Side = IsWhite ? 0 : 1;
    constexpr int promotionRank = IsWhite ? 0 : 7;
    constexpr int prePromotionRank = IsWhite ? 1 : 6;
    constexpr int pawnPushDelta = IsWhite ? -8 : 8;

    while (pawnBB) {
        const int from = engine::popLSB(pawnBB);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const uint64_t pawnAttacks = pieces::PAWN_ATTACKS[Side][from];
        uint64_t attacks = pawnAttacks & (oppOcc | enPassantBit);

        // Add forward push moves for promotion
        if (chess::Board::rank(from) == prePromotionRank) {
            const int frontSq = from + pawnPushDelta;
            if ((chess::Board::bitMask(frontSq) & occ) == 0) {
                attacks |= chess::Board::bitMask(frontSq);
            }
        }

        if (pinnedMask & fromBit) {
            attacks &= pinRayBySquare[from];
        }

        while (attacks) {
            const int to = engine::popLSB(attacks);
            const bool isEnPassant = (to == enPassantIndex) && (enPassantBit != 0ULL);

            if (isEnPassant && !b.isLegalPseudoMove(from, to, pawnPiece, false)) continue;

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
    return (b.getActiveColor() == chess::Board::WHITE)
        ? generateLegalMovesFor<true>(b, inCheckKnown, inCheckValue, inDoubleCheckValue)
        : generateLegalMovesFor<false>(b, inCheckKnown, inCheckValue, inDoubleCheckValue);
}

template<bool IsWhite>
MoveList<chess::Board::Move> MoveGenerator::generateLegalMovesFor(const chess::Board& b, bool inCheckKnown,
                                                                  bool inCheckValue, bool inDoubleCheckValue) noexcept {
    // Macro-step 1: Initialize side-to-move context and occupancy masks.
    MoveList<chess::Board::Move> moves;

    constexpr uint8_t color = IsWhite ? chess::Board::WHITE : chess::Board::BLACK;
    constexpr int side = IsWhite ? 0 : 1;
    
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
    const uint8_t kingPiece = chess::Board::KING | color;

    // Macro-step 2: Compute check-evasion mask when in single-check.
    uint64_t evasionMask = ~0ULL;
    if (singleCheck) {
        computeCheckEvasionMasks<IsWhite>(b, evasionMask);
    }

    if (!kings) [[unlikely]] return moves;

    const int kingFrom = __builtin_ctzll(kings);
    const chess::Coords kingFromC{static_cast<uint8_t>(kingFrom)};

    uint64_t mask = pieces::KING_ATTACKS[kingFrom] & ~ownOcc;
    while (mask) {
        const int to = engine::popLSB(mask);
        if (b.isLegalPseudoMove(kingFrom, to, kingPiece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{kingFromC, chess::Coords{static_cast<uint8_t>(to)}});
        }
    }

    if (!inCheck) { // castling: illegal when in check.
        const int f = chess::Board::file(kingFrom);
        if (f <= 5 && b.isLegalPseudoMove(kingFrom, kingFrom + 2, kingPiece)) {
            const int castleTo = kingFrom + 2;
            moves.emplace_back(chess::Board::Move{kingFromC, chess::Coords{static_cast<uint8_t>(castleTo)}});
        }
        if (f >= 2 && b.isLegalPseudoMove(kingFrom, kingFrom - 2, kingPiece)) {
            const int castleTo = kingFrom - 2;
            moves.emplace_back(chess::Board::Move{kingFromC, chess::Coords{static_cast<uint8_t>(castleTo)}});
        }
    }
    // In double-check only king moves are legal.
    if (inDoubleCheck) return moves;

    // Macro-step 4: Compute pin rays to restrict non-king piece mobility.
    // NOTE: for performance, legality checks are skipped for many non-king moves
    // when check/pin filters already guarantee king safety.
    uint64_t pinnedMask = 0ULL;
    // NOTE: per performance, don't zero-initialize this array.
    // It's only ever read where pinnedMask has a bit set.
    std::array<uint64_t, 64> pinRayBySquare;
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays<IsWhite>(b, kingFromC, pinnedMask, pinRayBySquare.data());
    }

    while (pawns) {
        const int from = engine::popLSB(pawns);
        const uint64_t fromBit = chess::Board::bitMask(from);
        uint64_t mask = pieces::getPawnForwardPushes(from, IsWhite, occ);
        const uint64_t epCandidate = (pieces::PAWN_ATTACKS[side][from] & enPassantBit) ? enPassantBit : 0ULL;
        mask |= (pieces::PAWN_ATTACKS[side][from] & oppOcc) | epCandidate;
        if (singleCheck) mask &= evasionMask;
        if (pinnedMask & fromBit) mask &= pinRayBySquare[from];
        // Keep EP candidate for legality check because EP changes occupancy on two squares.
        mask |= epCandidate;
        addPawnMovesFromMask<IsWhite>(
            b, moves, from, mask, inCheck, inDoubleCheck, pawnPiece,
            enPassant, hasEnPassant);
    }    
    
    if (singleCheck) {
        if (pinnedMask) {
            generateNonPawnLegalMoves<true, true, chess::Board::KNIGHT>(
                moves, knights, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<true, true, chess::Board::BISHOP>(
                moves, bishops, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<true, true, chess::Board::ROOK>(
                moves, rooks, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<true, true, chess::Board::QUEEN>(
                moves, queens, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        } else {
            generateNonPawnLegalMoves<false, true, chess::Board::KNIGHT>(
                moves, knights, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<false, true, chess::Board::BISHOP>(
                moves, bishops, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<false, true, chess::Board::ROOK>(
                moves, rooks, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<false, true, chess::Board::QUEEN>(
                moves, queens, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        }
    } else {
        if (pinnedMask) {
            generateNonPawnLegalMoves<true, false, chess::Board::KNIGHT>(
                moves, knights, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<true, false, chess::Board::BISHOP>(
                moves, bishops, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<true, false, chess::Board::ROOK>(
                moves, rooks, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<true, false, chess::Board::QUEEN>(
                moves, queens, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
        } else {
            generateNonPawnLegalMoves<false, false, chess::Board::KNIGHT>(
                moves, knights, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<false, false, chess::Board::BISHOP>(
                moves, bishops, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<false, false, chess::Board::ROOK>(
                moves, rooks, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
            generateNonPawnLegalMoves<false, false, chess::Board::QUEEN>(
                moves, queens, occ, ownOcc, 0ULL, pinnedMask, pinRayBySquare.data());
        }
    }

    return moves;
}

MoveList<chess::Board::Move> MoveGenerator::generateLegalEvasions(
    const chess::Board& b,
    bool inDoubleCheckKnown,
    bool inDoubleCheckValue) noexcept {
    return (b.getActiveColor() == chess::Board::WHITE)
        ? generateLegalEvasionsFor<true>(b, inDoubleCheckKnown, inDoubleCheckValue)
        : generateLegalEvasionsFor<false>(b, inDoubleCheckKnown, inDoubleCheckValue);
}

template<bool IsWhite>
MoveList<chess::Board::Move> MoveGenerator::generateLegalEvasionsFor(
    const chess::Board& b,
    bool inDoubleCheckKnown,
    bool inDoubleCheckValue) noexcept {
    MoveList<chess::Board::Move> moves;

    constexpr uint8_t color = IsWhite ? chess::Board::WHITE : chess::Board::BLACK;
    constexpr int side = IsWhite ? 0 : 1;

    const uint64_t occ = b.getPiecesBitMap();
    uint64_t pawns = b.pawns_bb[side];
    const uint64_t knights = b.knights_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    const uint64_t rooks = b.rooks_bb[side];
    const uint64_t queens = b.queens_bb[side];
    const uint64_t kings = b.kings_bb[side];

    if (!kings) [[unlikely]] return moves;

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;
    const uint64_t oppOcc = occ & ~ownOcc;
    const chess::Coords enPassant = b.getEnPassant();
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const uint64_t enPassantBit = hasEnPassant ? chess::Board::bitMask(enPassant.index) : 0ULL;
    const bool inDoubleCheck = inDoubleCheckKnown ? inDoubleCheckValue : b.isDoubleCheck(color);
    const bool singleCheck = !inDoubleCheck;
    const uint8_t pawnPiece = chess::Board::PAWN | color;
    const uint8_t kingPiece = chess::Board::KING | color;

    uint64_t evasionMask = ~0ULL;
    if (singleCheck) {
        computeCheckEvasionMasks<IsWhite>(b, evasionMask);
    }

    const int kingFrom = __builtin_ctzll(kings);
    const chess::Coords kingFromC{static_cast<uint8_t>(kingFrom)};
    uint64_t kingTargets = pieces::KING_ATTACKS[kingFrom] & ~ownOcc;
    while (kingTargets) {
        const int to = engine::popLSB(kingTargets);
        if (b.isLegalPseudoMove(kingFrom, to, kingPiece, true, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{kingFromC, chess::Coords{static_cast<uint8_t>(to)}});
        }
    }

    if (inDoubleCheck) return moves;

    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare;
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays<IsWhite>(b, kingFromC, pinnedMask, pinRayBySquare.data());
    }

    while (pawns) {
        const int from = engine::popLSB(pawns);
        const uint64_t fromBit = chess::Board::bitMask(from);
        uint64_t mask = pieces::getPawnForwardPushes(from, IsWhite, occ);
        const uint64_t epCandidate = (pieces::PAWN_ATTACKS[side][from] & enPassantBit) ? enPassantBit : 0ULL;
        mask |= (pieces::PAWN_ATTACKS[side][from] & oppOcc) | epCandidate;
        mask &= evasionMask;
        if (pinnedMask & fromBit) mask &= pinRayBySquare[from];
        mask |= epCandidate;
        addPawnMovesFromMask<IsWhite>(
            b, moves, from, mask, true, false, pawnPiece,
            enPassant, hasEnPassant);
    }

    if (pinnedMask) {
        generateNonPawnLegalMoves<true, true, chess::Board::KNIGHT>(
            moves, knights, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<true, true, chess::Board::BISHOP>(
            moves, bishops, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<true, true, chess::Board::ROOK>(
            moves, rooks, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<true, true, chess::Board::QUEEN>(
            moves, queens, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
    } else {
        generateNonPawnLegalMoves<false, true, chess::Board::KNIGHT>(
            moves, knights, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<false, true, chess::Board::BISHOP>(
            moves, bishops, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<false, true, chess::Board::ROOK>(
            moves, rooks, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
        generateNonPawnLegalMoves<false, true, chess::Board::QUEEN>(
            moves, queens, occ, ownOcc, evasionMask, pinnedMask, pinRayBySquare.data());
    }

    return moves;
}

MoveList<chess::Board::Move> MoveGenerator::generateTacticalMoves(const chess::Board& b) noexcept {
    return (b.getActiveColor() == chess::Board::WHITE)
        ? generateTacticalMovesFor<true>(b)
        : generateTacticalMovesFor<false>(b);
}

template<bool IsWhite>
MoveList<chess::Board::Move> MoveGenerator::generateTacticalMovesFor(const chess::Board& b) noexcept {
    MoveList<chess::Board::Move> moves;

    constexpr uint8_t color = IsWhite ? chess::Board::WHITE : chess::Board::BLACK;
    constexpr int side = IsWhite ? 0 : 1;

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
    const int kingIndex = __builtin_ctzll(kings);

    uint64_t pinnedMask = 0ULL;
    // NOTE: per performance, don't zero-initialize this array.
    std::array<uint64_t, 64> pinRayBySquare;
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays<IsWhite>(b, chess::Coords{static_cast<uint8_t>(kingIndex)}, pinnedMask, pinRayBySquare.data());
    }

    const int enPassantIndex = hasEnPassant ? enPassant.index : 0;
    appendPawnTacticalNoChecks<IsWhite>(
        b, moves, pawns, occ, oppOcc, enPassantBit, enPassantIndex,
        pinnedMask, pinRayBySquare.data(), pawnPiece);
    
    if (pinnedMask) {
        appendNonPawnTacticalNoChecks<true, chess::Board::KNIGHT>(moves, knights, occ, oppOcc, pinnedMask, pinRayBySquare.data());
        appendNonPawnTacticalNoChecks<true, chess::Board::BISHOP>(moves, bishops, occ, oppOcc, pinnedMask, pinRayBySquare.data());
        appendNonPawnTacticalNoChecks<true, chess::Board::ROOK>(moves, rooks, occ, oppOcc, pinnedMask, pinRayBySquare.data());
        appendNonPawnTacticalNoChecks<true, chess::Board::QUEEN>(moves, queens, occ, oppOcc, pinnedMask, pinRayBySquare.data());
    } else {
        appendNonPawnTacticalNoChecks<false, chess::Board::KNIGHT>(moves, knights, occ, oppOcc, pinnedMask, pinRayBySquare.data());
        appendNonPawnTacticalNoChecks<false, chess::Board::BISHOP>(moves, bishops, occ, oppOcc, pinnedMask, pinRayBySquare.data());
        appendNonPawnTacticalNoChecks<false, chess::Board::ROOK>(moves, rooks, occ, oppOcc, pinnedMask, pinRayBySquare.data());
        appendNonPawnTacticalNoChecks<false, chess::Board::QUEEN>(moves, queens, occ, oppOcc, pinnedMask, pinRayBySquare.data());
    }

    uint64_t kingAttacks = pieces::KING_ATTACKS[kingIndex] & oppOcc;
    while (kingAttacks) {
        appendMoveByIndex(moves, kingIndex, engine::popLSB(kingAttacks));
    }
    return moves;
}

engine::Sorter::MovePickerData MoveGenerator::generateQSearchEvasions(
    const chess::Board& b,
    bool inDoubleCheckKnown,
    bool inDoubleCheckValue) noexcept {
    MoveList<chess::Board::Move> evasions = generateLegalEvasions(b, inDoubleCheckKnown, inDoubleCheckValue);
    if (evasions.is_empty()) return engine::Sorter::MovePickerData{};

    engine::Sorter::MovePickerData data;
    data.moves = engine::Sorter::sortEvasionsForcingFirst(std::move(evasions), b);
    data.size = data.moves.size;
    data.currentIndex = 0;
    return data;
}

engine::Sorter::MovePickerData MoveGenerator::generateQSearchTacticalMoves(
    const chess::Board& b,
    int32_t standPat,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool usIsWhite) noexcept {
    MoveList<chess::Board::Move> tacticalMoves = generateTacticalMoves(b);
    if (tacticalMoves.is_empty()) return engine::Sorter::MovePickerData{};
    return engine::Sorter::sortTacticalMoves(tacticalMoves, b, standPat, alpha, beta, ply, usIsWhite);
}

template<bool IsWhite>
void MoveGenerator::addPawnMovesFromMask(const chess::Board& b, MoveList<chess::Board::Move>& moves,
                                         int from, uint64_t mask, bool inCheck, bool inDoubleCheck,
                                         uint8_t pawnPiece,
                                         chess::Coords enPassant, bool hasEnPassant) noexcept {
    if (!mask) [[unlikely]] return;

    constexpr int promotionRank = IsWhite ? 0 : 7;
    const int fromFile = chess::Board::file(from);

    while (mask) {
        const int to = engine::popLSB(mask);
        const bool isEnPassant = hasEnPassant
            && (to == enPassant.index)
            && (chess::Board::file(to) != fromFile);

        if (isEnPassant && !b.isLegalPseudoMove(from, to, pawnPiece, inCheck, inDoubleCheck)) {
            continue;
        }

        if (chess::Board::rank(to) == promotionRank) {
            appendPromotionSetByIndex(moves, from, to);
        } else {
            moves.emplace_back();
            auto& m = moves[moves.size - 1];
            m.from.index = from;
            m.to.index   = to;
            m.promotionPiece = '\0';
        }
    }
}

template<bool IsWhite>
void MoveGenerator::computePinRays(const chess::Board& b, chess::Coords kingPos,
                                   uint64_t& pinnedMask, uint64_t pinRays[64]) noexcept {
    pinnedMask = 0ULL;
    constexpr int us = IsWhite ? 0 : 1;
    constexpr int them = us ^ 1;
    const int kingSq = kingPos.index;

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
        const int pinnerSq = engine::popLSB(rookPinners);
        const uint64_t between = BETWEEN_EXCLUSIVE_LUT[kingSq][pinnerSq];
        const uint64_t blockers = between & ownOcc;
        if (blockers && ((blockers & (blockers - 1)) == 0ULL)) {
            const int pinnedSq = __builtin_ctzll(blockers);
            pinnedMask |= blockers;
            pinRays[pinnedSq] = between | chess::Board::bitMask(pinnerSq);
        }
    }

    while (bishopPinners) {
        const int pinnerSq = engine::popLSB(bishopPinners);
        const uint64_t between = BETWEEN_EXCLUSIVE_LUT[kingSq][pinnerSq];
        const uint64_t blockers = between & ownOcc;
        if (blockers && ((blockers & (blockers - 1)) == 0ULL)) {
            const int pinnedSq = __builtin_ctzll(blockers);
            pinnedMask |= blockers;
            pinRays[pinnedSq] = between | chess::Board::bitMask(pinnerSq);
        }
    }
}

template<bool IsWhite>
void MoveGenerator::computeCheckEvasionMasks(
    const chess::Board& b,
    uint64_t& evasionMask) noexcept {
    evasionMask = 0ULL;

    constexpr int us = IsWhite ? 0 : 1;
    constexpr int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        return;
    }
  
    const int kingSq = __builtin_ctzll(kingBB);
    const uint64_t occ = b.getPiecesBitMap();

    const uint64_t rookCheckers = pieces::getRookAttacks(kingSq, occ) & (b.rooks_bb[them] | b.queens_bb[them]);
    const uint64_t bishopCheckers = pieces::getBishopAttacks(kingSq, occ) & (b.bishops_bb[them] | b.queens_bb[them]);
    const uint64_t checkersMask =
        (pieces::PAWN_ATTACKERS_TO[them][kingSq] & b.pawns_bb[them])
        | (pieces::KNIGHT_ATTACKS[kingSq] & b.knights_bb[them])
        | (pieces::KING_ATTACKS[kingSq] & b.kings_bb[them])
        | rookCheckers
        | bishopCheckers;

    if ((checkersMask & (checkersMask - 1)) != 0ULL) {
        return;
    }

    if (!checkersMask) [[unlikely]] {
        evasionMask = ~0ULL;
        return;
    }

    const int checkerSq = __builtin_ctzll(checkersMask);
    const uint64_t checkerBit = chess::Board::bitMask(checkerSq);
    evasionMask = checkerBit;
    if ((rookCheckers | bishopCheckers) & checkerBit) {
        evasionMask |= BETWEEN_EXCLUSIVE_LUT[kingSq][checkerSq];
    }
}



template<bool HasPins, bool InCheck, uint8_t PieceType>
void MoveGenerator::generateNonPawnLegalMoves(
    MoveList<chess::Board::Move>& moves,
    uint64_t bb,
    uint64_t occ,
    uint64_t ownOcc,
    uint64_t evasionMask,
    uint64_t pinnedMask,
    const uint64_t pinRayBySquare[64]) noexcept {
    if constexpr (!HasPins) {
        (void)pinnedMask;
        (void)pinRayBySquare;
    }
    if constexpr (!InCheck) {
        (void)evasionMask;
    }

    while (bb) {
        const int from = engine::popLSB(bb);
        uint64_t mask = pieces::generateMovesByType<PieceType>(from, occ) & ~ownOcc;
        if constexpr (InCheck) {
            mask &= evasionMask;
        }
        if constexpr (HasPins) {
            if (pinnedMask & chess::Board::bitMask(from)) mask &= pinRayBySquare[from];
        }

        while (mask) {
            appendMoveByIndex(moves, from, engine::popLSB(mask));
        }
    }
}

} // namespace engine
