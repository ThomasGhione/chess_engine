#include "../engine.hpp"
#include "../tt.hpp"

namespace engine {

static void addNonPawnMovesFromMaskFast(const chess::Board& b,
                                        MoveList<chess::Board::Move>& moves,
                                        uint8_t from,
                                        uint64_t mask,
                                        bool inCheck,
                                        bool inDoubleCheck,
                                        uint8_t fromPiece,
                                        bool skipLegalityCheck) noexcept;
static void addPawnMovesFromMaskFast(const chess::Board& b,
                                     MoveList<chess::Board::Move>& moves,
                                     uint8_t from,
                                     uint64_t mask,
                                     bool inCheck,
                                     bool inDoubleCheck,
                                     uint8_t fromPiece,
                                     bool skipLegalityCheck,
                                     uint8_t promotionRank,
                                     const chess::Coords& enPassant) noexcept;

// Builds a bitboard containing only squares between from and to:
// If from == to, or they are not aligned (same file, rank, or diagonal), returns 0.
// If aligned, it walks along the direction and sets intermediate squares.
// Explicitly excludes target square to (and also excludes from).
static inline uint64_t betweenMaskExclusive(uint8_t from, uint8_t to) noexcept {
    if (from == to) return 0ULL;

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

    uint64_t mask = 0ULL;
    int f = fromFile + stepFile;
    int r = fromRank + stepRank;
    while (f != toFile || r != toRank) {
        mask |= chess::Board::bitMask(static_cast<uint8_t>((r << 3) | f));
        f += stepFile;
        r += stepRank;
    }

    // Exclude target square: caller adds checker square explicitly when needed.
    mask &= ~chess::Board::bitMask(to);
    return mask;
}

// Ritorna una maschera con i bit delle case su cui si può muovere o intercettare
// to evade check (evasionMask).
static void computeCheckEvasionMasks(const chess::Board& b,
                                     uint8_t activeColor,
                                     bool inCheck,
                                     bool inDoubleCheck,
                                     uint64_t& outEvasionMask) noexcept {
    outEvasionMask = ~0ULL;

    if (!inCheck) return;

    const int us = chess::Board::colorToIndex(activeColor);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        outEvasionMask = 0ULL;
        return;
    }

    const uint8_t kingSq = static_cast<uint8_t>(__builtin_ctzll(kingBB));
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t checkersMask = 0ULL;
    checkersMask |= pieces::PAWN_ATTACKERS_TO[us][kingSq] & b.pawns_bb[them];
    checkersMask |= pieces::KNIGHT_ATTACKS[kingSq] & b.knights_bb[them];
    checkersMask |= pieces::KING_ATTACKS[kingSq] & b.kings_bb[them];
    checkersMask |= pieces::getRookAttacks(kingSq, occ) & (b.rooks_bb[them] | b.queens_bb[them]);
    checkersMask |= pieces::getBishopAttacks(kingSq, occ) & (b.bishops_bb[them] | b.queens_bb[them]);

    if (inDoubleCheck || __builtin_popcountll(checkersMask) > 1) {
        outEvasionMask = 0ULL;
        return;
    }

    if (!checkersMask) [[unlikely]] {
        outEvasionMask = ~0ULL;
        return;
    }

    const uint8_t checkerSq = static_cast<uint8_t>(__builtin_ctzll(checkersMask));
    const uint8_t checkerType = b.get(checkerSq) & chess::Board::MASK_PIECE_TYPE;

    outEvasionMask = chess::Board::bitMask(checkerSq);
    if (checkerType == chess::Board::ROOK
        || checkerType == chess::Board::BISHOP
        || checkerType == chess::Board::QUEEN) {
        outEvasionMask |= betweenMaskExclusive(kingSq, checkerSq);
    }
}

// Returns a mask with bits for pieces pinned to the king (pinnedMask)
// e un array che per ogni casa contiene la maschera del raggio di pin (pinRayBySquare).
static void computePinRays(const chess::Board& b,
                           uint8_t activeColor,
                           uint64_t& outPinnedMask,
                           std::array<uint64_t, 64>& outPinRayBySquare) noexcept {
    outPinnedMask = 0ULL;
    outPinRayBySquare.fill(0ULL);

    const int us = chess::Board::colorToIndex(activeColor);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        return;
    }

    const uint64_t rookLikeEnemy = b.rooks_bb[them] | b.queens_bb[them];
    const uint64_t bishopLikeEnemy = b.bishops_bb[them] | b.queens_bb[them];
    if ((rookLikeEnemy | bishopLikeEnemy) == 0ULL) [[likely]] {
        return;
    }

    const uint8_t kingSq = static_cast<uint8_t>(__builtin_ctzll(kingBB));
    // Fast bailout: if no enemy slider is even geometrically aligned with the king,
    // no pin can exist and we can skip directional scans entirely.
    if (((pieces::getRookAttacks(kingSq, 0ULL) & rookLikeEnemy) |
         (pieces::getBishopAttacks(kingSq, 0ULL) & bishopLikeEnemy)) == 0ULL) [[likely]] {
        return;
    }
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
            if (pieceColor == activeColor) {
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
                    outPinnedMask |= chess::Board::bitMask(static_cast<uint8_t>(pinnedSq));
                    outPinRayBySquare[static_cast<size_t>(pinnedSq)] =
                        betweenMaskExclusive(kingSq, sq) | chess::Board::bitMask(sq);
                }
            }
            break;
        }
    }
}

static void addTacticalMovesFromMask(const chess::Board& b,
                                     MoveList<chess::Board::Move>& moves,
                                     uint8_t from,
                                     uint64_t mask,
                                     bool isPawn,
                                     bool isWhiteToMove,
                                     bool includeChecks,
                                     const chess::Coords& enPassant,
                                     bool inCheck,
                                     bool inDoubleCheck,
                                     uint8_t fromPiece,
                                     bool skipLegalityCheck) noexcept {
    const chess::Coords fromC{from};
    const uint8_t oppColor = includeChecks
        ? chess::Board::oppositeColor(b.getActiveColor())
        : static_cast<uint8_t>(chess::Board::WHITE);
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        const uint8_t toPiece = b.get(to);
        const bool isEnPassant = isPawn && hasEnPassant && (to == enPassant.index)
            && (static_cast<uint8_t>(to & 7) != fromFile) && (toPiece == chess::Board::EMPTY);
        const bool isCapture = (toPiece != chess::Board::EMPTY) || isEnPassant;
        const bool isPromotion = isPawn && (chess::Board::rankOf(to) == chess::Board::promotionRank(isWhiteToMove));

        // Fast path: when checks are disabled, skip non-tactical quiet moves immediately.
        if (!includeChecks && !isCapture && !isPromotion) {
            continue;
        }

        const chess::Coords toC{to};

        if (!skipLegalityCheck || isEnPassant) {
            if (!b.isLegalPseudoMove(from, to, fromPiece, inCheck, inDoubleCheck)) {
                continue;
            }
        }

        if (isPromotion) {
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
            continue;
        }

        bool shouldAdd = isCapture;

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

// Specialized helper for tactical generation in in-check path:
// - inCheck=true, inDoubleCheck=false
// - skipLegalityCheck is never allowed
// This removes generic branches from the hot loop used by qsearch evasions.
static inline void addTacticalMovesFromMaskInCheck(const chess::Board& b,
                                                   MoveList<chess::Board::Move>& moves,
                                                   uint8_t from,
                                                   uint64_t mask,
                                                   bool isPawn,
                                                   bool isWhiteToMove,
                                                   const chess::Coords& enPassant,
                                                   uint8_t fromPiece) noexcept {
    if (!mask) [[unlikely]] return;
    const chess::Coords fromC{from};
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        const uint8_t toPiece = b.get(to);
        const bool isEnPassant = isPawn && hasEnPassant && (to == enPassant.index)
            && (static_cast<uint8_t>(to & 7) != fromFile) && (toPiece == chess::Board::EMPTY);
        const bool isCapture = (toPiece != chess::Board::EMPTY) || isEnPassant;
        const bool isPromotion = isPawn && (chess::Board::rankOf(to) == chess::Board::promotionRank(isWhiteToMove));

        if (!isCapture && !isPromotion) {
            continue;
        }

        if (!b.isLegalPseudoMove(from, to, fromPiece, true, false)) {
            continue;
        }

        const chess::Coords toC{to};
        if (isPromotion) {
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
        } else {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}

MoveList<chess::Board::Move>
Engine::generateLegalMoves(const chess::Board& b) noexcept {
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
    const uint8_t promotionRank = chess::Board::promotionRank(isWhite);
    const uint8_t pawnPiece = static_cast<uint8_t>(chess::Board::PAWN | color);
    const uint8_t knightPiece = static_cast<uint8_t>(chess::Board::KNIGHT | color);
    const uint8_t bishopPiece = static_cast<uint8_t>(chess::Board::BISHOP | color);
    const uint8_t rookPiece = static_cast<uint8_t>(chess::Board::ROOK | color);
    const uint8_t queenPiece = static_cast<uint8_t>(chess::Board::QUEEN | color);
    const uint8_t kingPiece = static_cast<uint8_t>(chess::Board::KING | color);
    uint64_t evasionMask = ~0ULL;
    if (singleCheck) {
        computeCheckEvasionMasks(b, color, inCheck, inDoubleCheck, evasionMask);
    }

    // ================= KING =================
    if (!kings) [[unlikely]] return moves; // No king found, return empty move list
    

    const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(kings));
    const chess::Coords fromC{from};

    // King moves MUST always check legality (can't move to attacked squares)
    uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;
    while (mask) {
        const uint8_t to = popLSB(mask);
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
    

    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare{};
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays(b, color, pinnedMask, pinRayBySquare);
    }
    

    // NOTE: for performance, legality checks are skipped for many non-king moves
    // when check/pin filters already guarantee king safety.
    
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        const bool skipLegalityCheck = !inCheck && !isPinned;
        uint64_t mask = pieces::getPawnForwardPushes(from, isWhite, occ);
        uint64_t caps = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
        uint64_t epCandidate = 0ULL;
        if (hasEnPassant && (pieces::PAWN_ATTACKS[isWhite][from] & enPassantBit)) {
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
        addPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck, pawnPiece,
                                 skipLegalityCheck, promotionRank, enPassant);
    }

    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        const bool skipLegalityCheck = !inCheck && !isPinned;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck,
                                    knightPiece, skipLegalityCheck);
    }

    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getBishopAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        const bool skipLegalityCheck = !inCheck && !isPinned;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck,
                                    bishopPiece, skipLegalityCheck);
    }

    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getRookAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        const bool skipLegalityCheck = !inCheck && !isPinned;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck,
                                    rookPiece, skipLegalityCheck);
    }

    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getQueenAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        const bool skipLegalityCheck = !inCheck && !isPinned;
        addNonPawnMovesFromMaskFast(b, moves, from, mask, inCheck, inDoubleCheck,
                                    queenPiece, skipLegalityCheck);
    }

    return moves;
}

static void addNonPawnMovesFromMaskFast(const chess::Board& b,
                                        MoveList<chess::Board::Move>& moves,
                                        uint8_t from,
                                        uint64_t mask,
                                        bool inCheck,
                                        bool inDoubleCheck,
                                        uint8_t fromPiece,
                                        bool skipLegalityCheck) noexcept {
    if (!mask) [[unlikely]] return;
    const chess::Coords fromC{from};
    if (skipLegalityCheck) {
        while (mask) {
            const uint8_t to = __builtin_ctzll(mask);
            mask &= (mask - 1);
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
        return;
    }

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        if (b.isLegalPseudoMove(from, to, fromPiece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }
}

static void addPawnMovesFromMaskFast(const chess::Board& b,
                                     MoveList<chess::Board::Move>& moves,
                                     uint8_t from,
                                     uint64_t mask,
                                     bool inCheck,
                                     bool inDoubleCheck,
                                     uint8_t fromPiece,
                                     bool skipLegalityCheck,
                                     uint8_t promotionRank,
                                     const chess::Coords& enPassant) noexcept {
    if (!mask) [[unlikely]] return;
    const chess::Coords fromC{from};
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        const chess::Coords toC{to};
        const bool isEnPassant = hasEnPassant
            && (toC == enPassant)
            && (static_cast<uint8_t>(to & 7) != fromFile);
        if (!skipLegalityCheck || isEnPassant) {
            if (!b.isLegalPseudoMove(from, to, fromPiece, inCheck, inDoubleCheck)) {
                continue;
            }
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
            addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                     enPassant, inCheck, inDoubleCheck, kingPiece, false);
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

            addTacticalMovesFromMask(b, moves, from, attacks, true, isWhite, includeChecks,
                                     enPassant, false, false, pawnPiece, skipLegalityCheck);
        }

        bb = knights;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::KNIGHT_ATTACKS[from] & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                     enPassant, false, false, knightPiece, !isPinned);
        }

        bb = bishops;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getBishopAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                     enPassant, false, false, bishopPiece, !isPinned);
        }

        bb = rooks;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getRookAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                     enPassant, false, false, rookPiece, !isPinned);
        }

        bb = queens;
        while (bb) {
            const uint8_t from = popLSB(bb);
            const uint64_t fromBit = chess::Board::bitMask(from);
            const bool isPinned = (pinnedMask & fromBit) != 0ULL;
            uint64_t attacks = pieces::getQueenAttacks(from, occ) & oppOcc;
            if (isPinned) attacks &= pinRayBySquare[from];
            addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                     enPassant, false, false, queenPiece, !isPinned);
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
                addTacticalMovesFromMaskInCheck(b, moves, from, attacks, true, isWhite, enPassant, pawnPiece);
            } else {
                addTacticalMovesFromMask(b, moves, from, attacks, true, isWhite, includeChecks,
                                         enPassant, true, false, pawnPiece, false);
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
                addTacticalMovesFromMaskInCheck(b, moves, from, attacks, false, isWhite, enPassant, knightPiece);
            } else {
                addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                         enPassant, true, false, knightPiece, false);
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
                addTacticalMovesFromMaskInCheck(b, moves, from, attacks, false, isWhite, enPassant, bishopPiece);
            } else {
                addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                         enPassant, true, false, bishopPiece, false);
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
                addTacticalMovesFromMaskInCheck(b, moves, from, attacks, false, isWhite, enPassant, rookPiece);
            } else {
                addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                         enPassant, true, false, rookPiece, false);
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
                addTacticalMovesFromMaskInCheck(b, moves, from, attacks, false, isWhite, enPassant, queenPiece);
            } else {
                addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                         enPassant, true, false, queenPiece, false);
            }
        }
    }

    // ================= KING (captures only) =================
    if (kings) {
        const uint8_t from = __builtin_ctzll(kings); // King: no need for poplsb (only one)
        uint64_t attacks = pieces::KING_ATTACKS[from] & oppOcc;
        addTacticalMovesFromMask(b, moves, from, attacks, false, isWhite, includeChecks,
                                 enPassant, inCheck, false, kingPiece, false);
    }

    return moves;
}

} // namespace engine
