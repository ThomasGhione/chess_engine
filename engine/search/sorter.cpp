#include "sorter.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>

namespace engine {

bool Sorter::sameFromTo(const chess::Board::Move& a, const chess::Board::Move& b) noexcept {
    // Macro-step 1: Compare source squares.
    // Macro-step 2: Compare destination squares.
    return a.from.index == b.from.index && a.to.index == b.to.index;
}

bool Sorter::sameFromTo(const chess::Board::Move& m, uint8_t from, uint8_t to) noexcept {
    // Macro-step 1: Compare source square against encoded `from`.
    // Macro-step 2: Compare destination square against encoded `to`.
    return m.from.index == from && m.to.index == to;
}

bool Sorter::containsMoveWithPromotion(
    const MoveList<chess::Board::Move>& moves,
    uint8_t from,
    uint8_t to,
    char promotionPiece) noexcept {
    // Macro-step 1: Scan the move list linearly.
    for (const auto& m : moves) {
        // Macro-step 2: Match coordinates and promotion piece exactly.
        if (sameFromTo(m, from, to) && m.promotionPiece == promotionPiece) {
            return true;
        }
    }

    // Macro-step 3: Return false when no exact match is present.
    return false;
}

bool Sorter::givesCheckAfterQuietMoveFast(
    const chess::Board& b,
    const chess::Board::Move& m,
    uint8_t fromPieceType,
    int usSide,
    uint8_t oppKingSq,
    uint64_t occ) noexcept {
    // Macro-step 1: Compute occupancy after the quiet move.
    const uint64_t fromBit = chess::Board::bitMask(m.from.index);
    const uint64_t toBit = chess::Board::bitMask(m.to.index);
    const uint64_t occAfter = (occ & ~fromBit) | toBit;

    // Macro-step 2: Mirror side bitboards with the moved piece relocated.
    uint64_t pawns = b.pawns_bb[usSide];
    uint64_t knights = b.knights_bb[usSide];
    uint64_t bishops = b.bishops_bb[usSide];
    uint64_t rooks = b.rooks_bb[usSide];
    uint64_t queens = b.queens_bb[usSide];
    uint64_t kings = b.kings_bb[usSide];

    switch (fromPieceType) {
        case chess::Board::PAWN:   pawns = (pawns & ~fromBit) | toBit; break;
        case chess::Board::KNIGHT: knights = (knights & ~fromBit) | toBit; break;
        case chess::Board::BISHOP: bishops = (bishops & ~fromBit) | toBit; break;
        case chess::Board::ROOK:   rooks = (rooks & ~fromBit) | toBit; break;
        case chess::Board::QUEEN:  queens = (queens & ~fromBit) | toBit; break;
        case chess::Board::KING:   kings = (kings & ~fromBit) | toBit; break;
        default: break;
    }

    // Macro-step 3: Test leaper attacks on the opponent king square.
    if (pieces::PAWN_ATTACKERS_TO[usSide][oppKingSq] & pawns) return true;
    if (pieces::KNIGHT_ATTACKS[oppKingSq] & knights) return true;
    if (pieces::KING_ATTACKS[oppKingSq] & kings) return true;

    // Macro-step 4: Test slider attacks with updated occupancy.
    const uint64_t rookLike = rooks | queens;
    if (rookLike && (pieces::getRookAttacks(oppKingSq, occAfter) & rookLike)) return true;
    const uint64_t bishopLike = bishops | queens;
    if (bishopLike && (pieces::getBishopAttacks(oppKingSq, occAfter) & bishopLike)) return true;

    // Macro-step 5: Report no check when no attack vector is active.
    return false;
}

int32_t Sorter::clampOrderingScore(int64_t score) noexcept {
    // Macro-step 1: Clamp to int32 max bound.
    if (score > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
        return std::numeric_limits<int32_t>::max();
    }

    // Macro-step 2: Clamp to int32 min bound.
    if (score < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) {
        return std::numeric_limits<int32_t>::min();
    }

    // Macro-step 3: Safe narrowing conversion.
    return static_cast<int32_t>(score);
}

int32_t Sorter::scoreMoveOrderingPriorityInline(
    chess::Board& b,
    const chess::Board::Move& m,
    uint8_t fromPieceType,
    bool isCapture,
    uint8_t victimType,
    int32_t see,
    bool isPromotionCandidate,
    int moveIndex,
    bool hashMoveIsLegal,
    uint8_t hashFrom,
    uint8_t hashTo,
    char hashPromo,
    int ply,
    const chess::Board::Move* previousMove,
    int usSide,
    uint8_t oppKingSq,
    uint64_t occ,
    bool usIsWhite,
    bool isEndgameOrdering,
    int fullMoveClock,
    const int16_t (&history)[2][64][64],
    const chess::Board::Move (&killerMoves)[2][MAX_PLY],
    const uint16_t (&counterMoves)[64][64],
    const int16_t (&captureHistory)[2][64][7][CAPTURE_HISTORY_SLOTS],
    const int32_t (&pieceValues)[8],
    int32_t orderingPenaltySamePawnOpening) noexcept {
    // =========================================================
    // MOVE ORDERING PRIORITY (from highest to lowest):
    // 1. Hash move (from TT) -> 100000
    // 2. Good captures (SEE >= 0) -> 10000 + MVV (1000-9000)
    // 3. Killer move 1 -> 9000
    // 4. Killer move 2 -> 8500
    // 5. Counter-move (response to prev move) -> 8200
    // 6. Checks (non-capture, lazy: first 8 moves) -> 8000
    // 7. Promotions (non-capture) -> 7000
    // 8. History heuristic -> 0-2000
    // 9. Bad captures (SEE < 0) -> -10000 + SEE
    // =========================================================

    // Macro-step 1: Give absolute top priority to a validated hash move.
    if (hashMoveIsLegal && sameFromTo(m, hashFrom, hashTo) && m.promotionPiece == hashPromo) {
        return 100000; // Highest priority
    }

    // Macro-step 2: Score captures with SEE + capture-history policy.
    if (isCapture) {
        // BAD CAPTURE: low priority, ordered by SEE value
        // Simpler single-tier approach: all bad captures get -10000 + SEE
        // Total: -10000 to -10001+ (worse SEE = lower priority)
        if (see < 0) {
            return clampOrderingScore(-10000 + see);
        }

        // GOOD CAPTURES: priority based on SEE + capture history
        int32_t score = 10000 + MVV_TABLE[victimType];

        // Add capture history bonus (0-500 range)
        const int32_t capHistPrimary = captureHistory[usSide][m.to.index][victimType][0];
        const int32_t capHistSecondary = captureHistory[usSide][m.to.index][victimType][1];
        const int32_t capHist = capHistPrimary + (capHistSecondary >> 1);
        score += std::min(static_cast<int32_t>(500), capHist / 20); // Scale down
        // Total: 10000-19500
        return clampOrderingScore(score);
    }

    // Macro-step 3: Score non-captures via killer/counter/check/promotion/history.

    // Check for killer moves FIRST (high priority)
    if (ply >= 0 && ply < MAX_PLY) {
        const auto& km1 = killerMoves[0][ply];
        const auto& km2 = killerMoves[1][ply];

        if (sameFromTo(m, km1)) {
            return 9000;
        }
        if (sameFromTo(m, km2)) {
            return 8500;
        }
    }

    // Check for counter-move (response to opponent's previous move)
    if (previousMove != nullptr && previousMove->from.index < 64) {
        const uint16_t counter = counterMoves[previousMove->from.index][previousMove->to.index];
        if (counter != 0
            && counter == tt::TranspositionTable::Entry::encodeMove(m.from.index, m.to.index, m.promotionPiece)) {
            return 8200; // Between killer moves and checks
        }
    }

    int32_t score = 0;

    // LAZY CHECK DETECTION: only for first 8 non-capture moves
    // Balances tactical strength with performance overhead
    if (moveIndex < 8 && oppKingSq < 64) {
        bool givesCheck = false;
        const bool isCastling = (fromPieceType == chess::Board::KING)
            && (std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index)) == 2);
        if (isPromotionCandidate || isCastling) {
            chess::Board::MoveState tmpState;
            doMoveWithPromotion(b, m, tmpState);
            givesCheck = b.inCheck(b.getActiveColor());
            b.undoMove(m, tmpState);
        } else {
            givesCheck = givesCheckAfterQuietMoveFast(
                b, m, fromPieceType, usSide, oppKingSq, occ);
        }

        if (givesCheck) {
            score = 8000; // High priority for checking moves
        }
    }

    // Macro-step 4: Apply quiet bonuses/penalties and history tie-breakers.

    // Promotion bonus (if it's not a capture and no check was already detected)
    if (score == 0 && isPromotionCandidate) {
        score = 7000;

        const char promo = static_cast<char>(std::tolower(static_cast<unsigned char>(m.promotionPiece)));
        uint8_t promoType = chess::Board::QUEEN; // default if promo char is missing
        if (promo == 'r') promoType = chess::Board::ROOK;
        else if (promo == 'b') promoType = chess::Board::BISHOP;
        else if (promo == 'n') promoType = chess::Board::KNIGHT;

        // Tie-break promotions naturally: Q > R > B > N
        score += pieceValues[promoType];
    }

    // Discourage placing a bishop directly in front of own pawn (blocks pawn advance)
    if (fromPieceType == chess::Board::BISHOP) {
        const int toIdx = m.to.index;
        const int behind = usIsWhite ? (toIdx - 8) : (toIdx + 8);
        if (behind >= 0 && behind < 64) {
            const uint64_t pawnMask = usIsWhite ? b.pawns_bb[0] : b.pawns_bb[1];
            if (pawnMask & chess::Board::bitMask(static_cast<uint8_t>(behind))) {
                int bishopBlockPenalty = 80;
                const int pawnFile = chess::Board::fileOf(static_cast<uint8_t>(behind));
                const int pawnRank = chess::Board::rankOf(static_cast<uint8_t>(behind));
                const int pawnStartRank = usIsWhite ? 6 : 1;
                // In opening, strongly de-prioritize bishop moves that sit in front of d/e pawns.
                if (fullMoveClock < 16 && (pawnFile == 3 || pawnFile == 4) && pawnRank == pawnStartRank) {
                    bishopBlockPenalty += 140;
                } else if (fullMoveClock < 16 && (pawnFile == 3 || pawnFile == 4)) {
                    bishopBlockPenalty += 70;
                }
                score -= bishopBlockPenalty;
            }
        }
    }

    // History heuristic (for regular quiet moves)
    if (score == 0 && ply >= 0 && ply < MAX_PLY) {
        int32_t histScore = history[usSide][m.from.index][m.to.index];
        // Map history to [-2000, 4000] range for better move differentiation
        // Negative history = moves that consistently fail = ordered below neutral
        score = std::min(static_cast<int32_t>(4000), std::max(static_cast<int32_t>(-2000), histScore));
    }

    // Macro-step 5: Apply pawn-specific ordering refinements and clamp.
    if (fromPieceType != chess::Board::PAWN) {
        return clampOrderingScore(score);
    }

    // In endgames prioritize pawn pushes slightly, especially advanced ones.
    // This is ordering-only: it does not force pushes, but avoids searching
    // king shuffles before obvious pawn-race candidates.
    if (isEndgameOrdering) {
        const int fromFile = chess::Board::fileOf(m.from.index);
        const int toFile = chess::Board::fileOf(m.to.index);
        if (fromFile == toFile) {
            const int toRank = chess::Board::rankOf(m.to.index);
            const int advancement = usIsWhite ? (6 - toRank) : (toRank - 1);
            if (advancement > 0) {
                score += 20 + advancement * 12;
            }
        }
        return clampOrderingScore(score);
    }

    // Discourage moving the same pawn twice in the opening: small negative ordering penalty
    // Simple heuristic: if the pawn is not on its starting rank in the opening, it's likely a second move
    if (fullMoveClock < 8) {
        const int fromRank = chess::Board::rankOf(m.from.index);
        const int pawnStartRank = usIsWhite ? 6 : 1; // white pawns start on rank index 6, black on 1
        if (fromRank != pawnStartRank) {
            score += orderingPenaltySamePawnOpening; // negative value lowers priority
        }
    }

    return clampOrderingScore(score);
}

uint8_t Sorter::getLeastValuableAttackerTo(
    const chess::Board& b,
    uint8_t sq,
    uint64_t occLocal,
    int sideLocal) noexcept {
    // Macro-step 1: Restrict bitboards to simulated occupancy.
    const uint64_t pawns_bb = b.pawns_bb[sideLocal] & occLocal;
    const uint64_t knights_bb = b.knights_bb[sideLocal] & occLocal;
    const uint64_t bishops_queens_bb = (b.bishops_bb[sideLocal] | b.queens_bb[sideLocal]) & occLocal;
    const uint64_t rooks_queens_bb = (b.rooks_bb[sideLocal] | b.queens_bb[sideLocal]) & occLocal;
    const uint64_t kings_bb = b.kings_bb[sideLocal] & occLocal;

    // Macro-step 2: Query attackers from least valuable to most valuable.
    uint64_t bb = pawns_bb & pieces::PAWN_ATTACKERS_TO[sideLocal][sq];
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    bb = knights_bb & pieces::KNIGHT_ATTACKS[sq];
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    bb = bishops_queens_bb & pieces::getBishopAttacks(sq, occLocal);
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    bb = rooks_queens_bb & pieces::getRookAttacks(sq, occLocal);
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    bb = kings_bb & pieces::KING_ATTACKS[sq];
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    // Macro-step 3: Return sentinel when no attacker exists.
    return 64; // no attacker
}

int32_t Sorter::staticExchangeEvaluation(
    const chess::Board& b,
    const chess::Board::Move& m) noexcept {
    // Macro-step 1: Initialize SEE state from move endpoints and side-to-move.
    const uint8_t toSq = m.to.index;
    const uint8_t fromSq = m.from.index;

    const int sideActive = chess::Board::colorToIndex(b.getActiveColor());
    const int sidePassive = sideActive ^ 1;

    // Value of the initially captured piece
    uint8_t capturedType = b.get(toSq) & chess::Board::MASK_PIECE_TYPE;
    if (capturedType == chess::Board::EMPTY) {
        // En passant: captures a pawn
        capturedType = chess::Board::PAWN;
    }

    // Canonical SEE (swap algorithm):
    // gain[0] = value(victim)
    // for each recapture i:
    //   gain[i] = value(captured_piece) - gain[i-1]
    // where captured_piece is the piece that just moved to the target square in the previous ply.
    constexpr int MAX_SEE_DEPTH = 16;
    int32_t gain[MAX_SEE_DEPTH];
    gain[0] = PIECE_VALUES[capturedType];

    // Macro-step 2: Build local occupancy after the initial capture move.
    uint64_t occ = b.getPiecesBitMap();
    occ ^= chess::Board::bitMask(fromSq); // remove the piece that makes the first capture from its square

    // After the initial move, the piece now "on target" is our initial attacker.
    uint8_t capturedOnTargetType = b.get(fromSq) & chess::Board::MASK_PIECE_TYPE;

    int depth = 1;
    int side = sidePassive; // the opponent captures next

    // Do not use piece-value early exits here.
    // They can misclassify captures like QxP as losing without checking
    // recaptures, pins, x-rays and overloaded defenders.
    while (depth < MAX_SEE_DEPTH) {
        // Find the least valuable attacker toward the target square
        uint8_t attacker = Sorter::getLeastValuableAttackerTo(b, toSq, occ, side);
        if (attacker == 64) break;

        // Determine attacker type using the piece bitboards AND the simulated occupancy
        // (safer than querying b.get(...) which reflects the original board only).
        const uint64_t attackerMask = chess::Board::bitMask(attacker);
        uint8_t currentAttackerType = chess::Board::PAWN; // default/fallback
        if ((b.pawns_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::PAWN;
        else if ((b.knights_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::KNIGHT;
        else if ((b.bishops_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::BISHOP;
        else if ((b.rooks_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::ROOK;
        else if ((b.queens_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::QUEEN;
        else if ((b.kings_bb[side] & occ & attackerMask) != 0) currentAttackerType = chess::Board::KING;

        // At this ply, capture the piece left on the target square
        // (i.e. the previous capturer).
        gain[depth] = PIECE_VALUES[capturedOnTargetType] - gain[depth - 1];

        // Remove the attacker from occupancy
        occ ^= attackerMask;

        // The piece that just captured now stays on target and can be
        // captured on the next ply.
        capturedOnTargetType = currentAttackerType;

        // Switch side
        side ^= 1;
        depth++;
    }

    // Macro-step 3: Negamax back-propagation of exchange gains.
    while (--depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }

    // Macro-step 4: Return root exchange score.
    return gain[0];
}

MoveList<chess::Board::Move> Sorter::sortLegalMoves(
    const MoveList<chess::Board::Move>& moves,
    int ply,
    chess::Board& b,
    bool usIsWhite,
    uint64_t hashKey,
    const int16_t (&history)[2][64][64],
    const chess::Board::Move (&killerMoves)[2][MAX_PLY],
    const uint16_t (&counterMoves)[64][64],
    const int16_t (&captureHistory)[2][64][7][CAPTURE_HISTORY_SLOTS],
    const tt::TranspositionTable* transpositionTable,
    const chess::Board::Move* previousMove,
    bool* outHashMoveIsLegal,
    int32_t orderingPenaltySamePawnOpening) noexcept {
    // Macro-step 1: Copy input list and early-out for empty input.
    MoveList<chess::Board::Move> sortedMoves = moves;
    if (sortedMoves.is_empty()) [[unlikely]] {
        if (outHashMoveIsLegal != nullptr) {
            *outHashMoveIsLegal = false;
        }
        return sortedMoves;
    }

    int32_t moveScores[MAX_MOVES] {};

    // Macro-step 2: Precompute board and phase features used by every move.
    const uint8_t activeColor = b.getActiveColor();
    const bool inCheck = b.inCheck(activeColor);
    const int fullMoveClock = b.getFullMoveClock();
    const int nonPawnMajors = __builtin_popcountll(
        b.knights_bb[0] | b.knights_bb[1] |
        b.bishops_bb[0] | b.bishops_bb[1] |
        b.rooks_bb[0]   | b.rooks_bb[1]   |
        b.queens_bb[0]  | b.queens_bb[1]);
    const bool isEndgameOrdering = (nonPawnMajors <= 5);
    const int usSide = chess::Board::colorBoolToIndex(usIsWhite);
    const int oppSide = usSide ^ 1;
    const uint64_t occ = b.getPiecesBitMap();
    const uint64_t oppKingBB = b.kings_bb[oppSide];
    const uint8_t oppKingSq = oppKingBB ? static_cast<uint8_t>(__builtin_ctzll(oppKingBB)) : 64;
    const uint8_t promotionRank = chess::Board::promotionRank(usIsWhite);
    const chess::Coords enPassant = b.getEnPassant();
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);

    // Macro-step 3: Probe and validate TT hash move.
    uint16_t encodedHashMove = 0;
    uint8_t hashFrom = 64;
    uint8_t hashTo = 64;
    char hashPromo = '\0';
    bool hashMoveIsLegal = false;

    // Probe TT with move-only API (no alpha/beta/score overhead).
    if (transpositionTable != nullptr && transpositionTable->probeMove(hashKey, encodedHashMove)) {
        tt::TranspositionTable::Entry::decodeMove(encodedHashMove, hashFrom, hashTo, hashPromo);

        // Validate hash move is in legal moves list (guards against TT collisions)
        hashMoveIsLegal = containsMoveWithPromotion(sortedMoves, hashFrom, hashTo, hashPromo);
    }

    // Macro-step 4: Score every move with copied ordering policy.
    for (int moveIndex = 0; moveIndex < sortedMoves.size; ++moveIndex) {
        const auto& m = sortedMoves[static_cast<size_t>(moveIndex)];
        const uint8_t fromPiece = b.get(m.from);
        const uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;

        const uint8_t toPiece = b.get(m.to);
        const uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isEpCapture = hasEnPassant
            && fromPieceType == chess::Board::PAWN
            && toPieceType == chess::Board::EMPTY
            && (m.to == enPassant)
            && (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index));
        const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
        const uint8_t victimType = isEpCapture ? static_cast<uint8_t>(chess::Board::PAWN) : toPieceType;
        const bool isPromotionCandidate = (fromPieceType == chess::Board::PAWN) && (m.to.rank() == promotionRank);
        const int32_t see = isCapture ? staticExchangeEvaluation(b, m) : 0;

        int32_t score = scoreMoveOrderingPriorityInline(
            b, m, fromPieceType, isCapture, victimType, see, isPromotionCandidate, moveIndex,
            hashMoveIsLegal, hashFrom, hashTo, hashPromo, ply, previousMove, usSide, oppKingSq, occ,
            usIsWhite, isEndgameOrdering, fullMoveClock, history, killerMoves, counterMoves,
            captureHistory, PIECE_VALUES, orderingPenaltySamePawnOpening);

        // NOTE: Stalemate check removed from move ordering (too expensive: doMove/undoMove per move!)
        // Stalemate is now handled ONLY in searchPosition() terminal node evaluation
        // This is much faster and still prevents stalemate in winning positions

        // King move penalties (lower king-move priority in the opening if not castling)
        if (fromPieceType == chess::Board::KING) {
            const int fileDelta = std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index));
            const bool isCastling = (fileDelta == 2);

            if (fullMoveClock < 10 && !inCheck && !isCastling) {
                score -= 220; // opening-only ordering penalty
            } else if (isCastling) {
                score += 550; // keep castling high priority without overpowering tactical quiets
            }
        }

        moveScores[moveIndex] = score;
    }

    // Macro-step 5: Run insertion-sort on score/move arrays in descending order.
    // Insertion-sort scores and moves together (descending score).
    // MAX_MOVES is small, and the list is often partially ordered.
    for (int i = 1; i < sortedMoves.size; ++i) {
        const chess::Board::Move keyMove = sortedMoves[static_cast<size_t>(i)];
        const int32_t keyScore = moveScores[i];
        int j = i - 1;
        while (j >= 0 && moveScores[j] < keyScore) {
            moveScores[j + 1] = moveScores[j];
            sortedMoves[static_cast<size_t>(j + 1)] = sortedMoves[static_cast<size_t>(j)];
            --j;
        }
        moveScores[j + 1] = keyScore;
        sortedMoves[static_cast<size_t>(j + 1)] = keyMove;
    }

    // Macro-step 6: Report whether hash move ended up as first move.
    const bool hashMovePlacedFirst = hashMoveIsLegal
        && !sortedMoves.is_empty()
        && sameFromTo(sortedMoves[0], hashFrom, hashTo)
        && sortedMoves[0].promotionPiece == hashPromo;

    if (outHashMoveIsLegal != nullptr) {
        *outHashMoveIsLegal = hashMovePlacedFirst;
    }

    return sortedMoves;
}

int32_t Sorter::clampQMoveScore(int64_t score) noexcept {
    // Macro-step 1: Clamp score to int32 max bound.
    if (score > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
        return std::numeric_limits<int32_t>::max();
    }

    // Macro-step 2: Clamp score to int32 min bound.
    if (score < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) {
        return std::numeric_limits<int32_t>::min();
    }

    // Macro-step 3: Safe narrowing conversion.
    return static_cast<int32_t>(score);
}

bool Sorter::shouldDeltaPrune(
    int32_t standPat,
    int32_t margin,
    int32_t alpha,
    int32_t beta,
    bool isWhite) noexcept {
    // Macro-step 1: Promote to int64 to avoid overflow in bound math.
    const int64_t standPat64 = static_cast<int64_t>(standPat);
    const int64_t margin64 = static_cast<int64_t>(margin);
    const int64_t alpha64 = static_cast<int64_t>(alpha);
    const int64_t beta64 = static_cast<int64_t>(beta);

    // Macro-step 2: Evaluate color-aware delta pruning condition.
    return isWhite ? (standPat64 + margin64 < alpha64) : (standPat64 - margin64 > beta64);
}

MoveList<chess::Board::Move> Sorter::sortTacticalMoves(
    const MoveList<chess::Board::Move>& tacticalMoves,
    const chess::Board& b,
    int32_t standPat,
    int32_t alpha,
    int32_t beta,
    int ply,
    bool usIsWhite,
    int32_t searchDepth) noexcept {
    // Macro-step 1: Copy the input tactical list and early-return when empty.
    MoveList<chess::Board::Move> sortedTacticalMoves = tacticalMoves;
    if (sortedTacticalMoves.is_empty()) {
        return sortedTacticalMoves;
    }

    // Sort tactical moves by MVV-LVA and SEE using compact parallel score storage.
    int32_t tacticalScores[MAX_MOVES] {};
    int filteredCount = 0;

    // Macro-step 2: Precompute shared metadata and dynamic pruning thresholds.
    const uint8_t promotionRank = chess::Board::promotionRank(usIsWhite);
    const chess::Coords enPassant = b.getEnPassant();
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);

    // Dynamic SEE pruning threshold based on depth and material balance
    // The engine should NOT speculate with losing captures hoping for positional comp
    // Shallow qsearch (ply < 10): SEE >= -15cp (only tiny tactical losses)
    // Mid qsearch (10-20): SEE >= -8cp (very conservative)
    // Deep qsearch (ply >= 20): SEE >= 0cp (neutral or better only)
    const int32_t seeThreshold = (ply < 10) ? -15 : ((ply < 20) ? -8 : 0);

    // Macro-step 3: Filter and score tactical moves with copied qsearch policy.
    for (int i = 0; i < sortedTacticalMoves.size; ++i) {
        const auto& m = sortedTacticalMoves[static_cast<size_t>(i)];
        const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
        const bool isPromotion = (fromPieceType == chess::Board::PAWN) && (m.to.rank() == promotionRank);
        const bool isEpCapture = hasEnPassant
            && fromPieceType == chess::Board::PAWN
            && toPieceType == chess::Board::EMPTY
            && (m.to == enPassant)
            && (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index));
        const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
        const uint8_t victimType = isEpCapture ? static_cast<uint8_t>(chess::Board::PAWN) : toPieceType;

        int32_t score = 0;

        if (isCapture) {
            // TODO test this better!!
            // ============================================================================
            // FUTILITY PRUNING IN QSEARCH
            // ============================================================================
            // Skip captures that can't possibly raise alpha, even if they win material.
            // This is aggressive pruning based on material value alone.
            const int32_t capturedValue = PIECE_VALUES[victimType];
            static constexpr int32_t FUTILITY_MARGIN = 100; // Minimal margin - prioritize material!

            // Check if this capture can possibly improve our position enough
            if (shouldDeltaPrune(standPat, capturedValue + FUTILITY_MARGIN, alpha, beta, usIsWhite)) {
                continue;
            }

            const int32_t see = staticExchangeEvaluation(b, m);

            // SEE-based pruning with dynamic threshold
            // Dynamic threshold is already strict enough (-16cp shallow, 0cp deep)
            // No need for additional hard cutoff (was redundant and too permissive at -300cp)
            if (see < seeThreshold) {
                continue;
            }

            // PER-MOVE DELTA PRUNING: prune captures that can't improve position
            // Even if this capture is "good" by SEE, if standPat + captureValue + margin
            // still can't reach alpha/beta, skip it
            static constexpr int32_t MOVE_DELTA_MARGIN = 100; // Minimal margin - material > position

            if (shouldDeltaPrune(standPat, see + MOVE_DELTA_MARGIN, alpha, beta, usIsWhite)) {
                continue; // Per-move delta pruning
            }

            // Score by MVV + SEE for better ordering
            // SEE-based ordering: captures with better SEE explored first
            score = clampQMoveScore(10000 + see + MVV_TABLE[victimType]);
            // Total: 10000 + see + MVV (1000-9000)
        } else {
            // Non-capture: must be a promotion
            if (isPromotion) {
                score = 9000; // Promotion (high priority)
            } else {
                continue;
            }
        }

        sortedTacticalMoves[static_cast<size_t>(filteredCount)] = m;
        tacticalScores[filteredCount] = score;
        ++filteredCount;
    }

    sortedTacticalMoves.size = filteredCount;

    // Macro-step 4: Return early if all tactical moves were pruned.
    // If all captures were pruned, return stand-pat
    if (sortedTacticalMoves.is_empty()) {
        return sortedTacticalMoves;
    }

    // Macro-step 5: Insertion-sort scored tactical moves in descending order.
    // Insertion-sort tactical moves + scores together (descending).
    for (int i = 1; i < sortedTacticalMoves.size; ++i) {
        const chess::Board::Move keyMove = sortedTacticalMoves[static_cast<size_t>(i)];
        const int32_t keyScore = tacticalScores[i];
        int j = i - 1;
        while (j >= 0 && tacticalScores[j] < keyScore) {
            tacticalScores[j + 1] = tacticalScores[j];
            sortedTacticalMoves[static_cast<size_t>(j + 1)] = sortedTacticalMoves[static_cast<size_t>(j)];
            --j;
        }
        tacticalScores[j + 1] = keyScore;
        sortedTacticalMoves[static_cast<size_t>(j + 1)] = keyMove;
    }

    // Macro-step 6: Return sorted tactical list.
    (void)searchDepth; // kept for API compatibility with qsearch context.
    return sortedTacticalMoves;
}

bool Sorter::isForcingEvasion(
    const chess::Board& b,
    const chess::Board::Move& m,
    const chess::Coords& enPassant,
    bool hasEnPassant) noexcept {
    // Macro-step 1: Captures are always forcing evasions.
    const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
    if (toPieceType != chess::Board::EMPTY) return true;

    // Macro-step 2: Non-pawn quiet moves are non-forcing here.
    const uint8_t fromPiece = b.get(m.from);
    const uint8_t fromType = fromPiece & chess::Board::MASK_PIECE_TYPE;
    if (fromType != chess::Board::PAWN) return false;

    // Macro-step 3: Promotions are forcing evasions.
    const bool isPromotion = (m.to.rank() == chess::Board::promotionRank(b.getColor(m.from) == chess::Board::WHITE));
    if (isPromotion) return true;

    // Macro-step 4: En-passant evasions are also forcing.
    return hasEnPassant
        && (m.to == enPassant)
        && (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index));
}

MoveList<chess::Board::Move> Sorter::sortEvasionsForcingFirst(
    const MoveList<chess::Board::Move>& evasions,
    const chess::Board& b) noexcept {
    // Macro-step 1: Prepare output list and shared en-passant state.
    MoveList<chess::Board::Move> orderedEvasions;
    const chess::Coords enPassant = b.getEnPassant();
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);

    // Macro-step 2: Reproduce two-pass evasion ordering (forcing first, quiet after).
    // Two-pass evasion ordering:
    // 1) forcing evasions (captures/promotions)
    // 2) quiet evasions.
    // This improves alpha-beta cutoffs in tactical check sequences.
    for (int pass = 0; pass < 2; ++pass) {
        const bool searchForcing = (pass == 0);
        for (const auto& m : evasions) {
            const bool isForcing = isForcingEvasion(b, m, enPassant, hasEnPassant);
            if (isForcing != searchForcing) {
                continue;
            }
            orderedEvasions.push_back(m);
        }
    }

    // Macro-step 3: Return ordered evasions.
    return orderedEvasions;
}

bool Sorter::isPromotionMove(
    const chess::Board& board,
    const chess::Board::Move& move) noexcept {
    // Macro-step 1: Fast-rank guard for non-promotion destinations.
    const uint8_t toRank = move.to.rank();
    if (toRank != 0 && toRank != 7) return false;

    // Macro-step 2: Ensure the moving piece is a pawn.
    const uint8_t piece = board.get(move.from);
    const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
    if (pieceType != chess::Board::PAWN) return false;

    // Macro-step 3: Check destination rank against side promotion rank.
    return toRank == chess::Board::promotionRank(board.getColor(move.from) == chess::Board::WHITE);
}

bool Sorter::doMoveWithPromotion(
    chess::Board& b,
    const chess::Board::Move& m,
    chess::Board::MoveState& state) noexcept {
    // Macro-step 1: Detect whether the move is a promotion.
    const bool isPromo = isPromotionMove(b, m);

    // Macro-step 2: Delegate promotion-choice normalization to Board::doMove internals.
    const char promoChoice = isPromo ? m.promotionPiece : '\0';

    // Macro-step 3: Execute move and return promotion flag.
    b.doMove(m, state, promoChoice);
    return isPromo;
}

} // namespace engine
