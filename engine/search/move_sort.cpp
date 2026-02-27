#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

static inline bool sameFromTo(const chess::Board::Move& a, const chess::Board::Move& b) noexcept {
    return a.from.index == b.from.index && a.to.index == b.to.index;
}

static inline bool sameFromTo(const chess::Board::Move& m, uint8_t from, uint8_t to) noexcept {
    return m.from.index == from && m.to.index == to;
}

static inline bool containsMoveWithPromotion(const MoveList<chess::Board::Move>& moves,
                                             uint8_t from,
                                             uint8_t to,
                                             char promotionPiece) noexcept {
    for (const auto& m : moves) {
        if (sameFromTo(m, from, to) && m.promotionPiece == promotionPiece) {
            return true;
        }
    }
    return false;
}

static inline bool givesCheckAfterQuietMoveFast(const chess::Board& b,
                                                const chess::Board::Move& m,
                                                uint8_t fromPieceType,
                                                int usSide,
                                                uint8_t oppKingSq,
                                                uint64_t occ) noexcept {
    const uint64_t fromBit = chess::Board::bitMask(m.from.index);
    const uint64_t toBit = chess::Board::bitMask(m.to.index);
    const uint64_t occAfter = (occ & ~fromBit) | toBit;

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

    if (pieces::PAWN_ATTACKERS_TO[usSide][oppKingSq] & pawns) return true;
    if (pieces::KNIGHT_ATTACKS[oppKingSq] & knights) return true;
    if (pieces::KING_ATTACKS[oppKingSq] & kings) return true;

    const uint64_t rookLike = rooks | queens;
    if (rookLike && (pieces::getRookAttacks(oppKingSq, occAfter) & rookLike)) return true;
    const uint64_t bishopLike = bishops | queens;
    if (bishopLike && (pieces::getBishopAttacks(oppKingSq, occAfter) & bishopLike)) return true;

    return false;
}

inline int64_t Engine::scoreMoveOrderingPriorityInline(
    chess::Board& b,
    const chess::Board::Move& m,
    uint8_t fromPieceType,
    bool isCapture,
    uint8_t victimType,
    int64_t see,
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
    const int32_t (&history)[2][64][64],
    const chess::Board::Move (&killerMoves)[2][64],
    const chess::Board::Move (&counterMoves)[64][64],
    const int32_t (&captureHistory)[2][64][7][2],
    const int64_t (&pieceValues)[8],
    int64_t orderingPenaltySamePawnOpening) noexcept {
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

    // Check if this is the hash move (highest priority!)
    // Only assign high priority if hash move was validated as legal
    if (hashMoveIsLegal && sameFromTo(m, hashFrom, hashTo) && m.promotionPiece == hashPromo) {
        return 100000; // Highest priority
    }

    if (isCapture) {

        // BAD CAPTURE: low priority, ordered by SEE value
        // Simpler single-tier approach: all bad captures get -10000 + SEE
        // Total: -10000 to -10001+ (worse SEE = lower priority)
        if (see < 0) {
            return -10000 + see;            
        }

        // GOOD CAPTURES: priority based on SEE + capture history
        int64_t score = 10000 + MVV_TABLE[victimType];
        
        // Add capture history bonus (0-500 range)
        const int64_t capHistPrimary = captureHistory[usSide][m.to.index][victimType][0];
        const int64_t capHistSecondary = captureHistory[usSide][m.to.index][victimType][1];
        const int64_t capHist = capHistPrimary + (capHistSecondary >> 1);
        score += std::min(static_cast<int64_t>(500), capHist / 20); // Scale down
        // Total: 10000-19500
        return score;
    }

    // NON-CAPTURES: killer, checks, history
    
    // Check for killer moves FIRST (high priority)
    if (ply >= 0 && ply < 64) {
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
        const auto& counter = counterMoves[previousMove->from.index][previousMove->to.index];
        if (counter.from.index < 64 && sameFromTo(m, counter)) {
            return 8200; // Between killer moves and checks
        }
    }

    int64_t score = 0;

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
            if (pawnMask & chess::Board::bitMask(behind)) {
                score += -80;
            }
        }
    }
    
    // History heuristic (for regular quiet moves)
    if (score == 0 && ply >= 0 && ply < 64) {
        int64_t histScore = history[usSide][m.from.index][m.to.index];
        // Map history to [-2000, 4000] range for better move differentiation
        // Negative history = moves that consistently fail = ordered below neutral
        score = std::min(static_cast<int64_t>(4000), std::max(static_cast<int64_t>(-2000), histScore));
    }

    if (fromPieceType != chess::Board::PAWN) {
        return score;
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
        return score;
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

    return score;
}

uint8_t Engine::getLeastValuableAttackerTo(const chess::Board& b, uint8_t sq, uint64_t occLocal, int sideLocal) noexcept {
    // Limit piece bitboards to the simulated occupancy so that pieces
    // that were "captured" in the simulated exchange aren't considered
    // as attackers in subsequent steps.
    const uint64_t pawns_bb = b.pawns_bb[sideLocal] & occLocal;
    const uint64_t knights_bb = b.knights_bb[sideLocal] & occLocal;
    const uint64_t bishops_queens_bb = (b.bishops_bb[sideLocal] | b.queens_bb[sideLocal]) & occLocal;
    const uint64_t rooks_queens_bb = (b.rooks_bb[sideLocal] | b.queens_bb[sideLocal]) & occLocal;
    const uint64_t kings_bb = b.kings_bb[sideLocal] & occLocal;

    uint64_t bb;

    // Pawns
    bb = pawns_bb & pieces::PAWN_ATTACKERS_TO[sideLocal][sq];
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    // Knights
    bb = knights_bb & pieces::KNIGHT_ATTACKS[sq];
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    // Bishops/Queens (diagonal) - compute bishop attacks only now
    bb = bishops_queens_bb & pieces::getBishopAttacks(sq, occLocal);
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    // Rooks/Queens (orthogonal) - compute rook attacks only if needed
    bb = rooks_queens_bb & pieces::getRookAttacks(sq, occLocal);
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    // Kings (last)
    bb = kings_bb & pieces::KING_ATTACKS[sq];
    if (bb) return static_cast<uint8_t>(__builtin_ctzll(bb));

    return 64; // no attacker
}

// Static Exchange Evaluation (SEE) - Quick version
int64_t Engine::staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept {
    const uint8_t toSq = m.to.index;
    const uint8_t fromSq = m.from.index;

    const int sideActive = b.getActiveColor() == chess::Board::WHITE ? 0 : 1;
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
    int64_t gain[MAX_SEE_DEPTH];
    gain[0] = PIECE_VALUES[capturedType];

    // Simulate the exchange on local occupancy
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
        uint8_t attacker = Engine::getLeastValuableAttackerTo(b, toSq, occ, side);
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

    // Negamax: propagate the best result backward
    while (--depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }

    return gain[0];
}

MoveList<Engine::ScoredMove> Engine::sortLegalMoves(
    const MoveList<chess::Board::Move>& moves,
    int ply,
    chess::Board& b,
    bool usIsWhite,
    uint64_t hashKey,
    const chess::Board::Move* previousMove) noexcept
{
    MoveList<ScoredMove> orderedScoredMoves;

    // Precompute expensive variables outside the loop
    const bool inCheck = b.inCheck(b.getActiveColor());
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

    // HASH MOVE: Retrieve from TT for highest priority
    uint16_t encodedHashMove = 0;
    uint8_t hashFrom = 64, hashTo = 64;
    char hashPromo = '\0';
    bool hashMoveIsLegal = false;
    
    // Probe TT to get hash move (don't care about score, just the move)
    int64_t dummyScore = 0;
    this->tt.probe(hashKey, 0, NEG_INF, POS_INF, dummyScore, encodedHashMove);
    
    if (encodedHashMove != 0) {
        tt::TranspositionTable::Entry::decodeMove(encodedHashMove, hashFrom, hashTo, hashPromo);

        // Validate hash move is in legal moves list (guards against TT collisions)
        hashMoveIsLegal = containsMoveWithPromotion(moves, hashFrom, hashTo, hashPromo);
    }

    int moveIndex = 0; // Track move count for lazy check detection
    for (const auto& m : moves) {
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
        const int64_t see = isCapture ? staticExchangeEvaluation(b, m) : 0;
        
        int64_t score = scoreMoveOrderingPriorityInline(
            b, m, fromPieceType, isCapture, victimType, see, isPromotionCandidate, moveIndex,
            hashMoveIsLegal, hashFrom, hashTo, hashPromo, ply, previousMove, usSide, oppKingSq, occ,
            usIsWhite, isEndgameOrdering, fullMoveClock, history, killerMoves, counterMoves,
            captureHistory, PIECE_VALUES, ORDERING_PENALTY_SAME_PAWN_OPENING);

        // NOTE: Stalemate check removed from move ordering (too expensive: doMove/undoMove per move!)
        // Stalemate is now handled ONLY in searchPosition() terminal node evaluation
        // This is much faster and still prevents stalemate in winning positions

        // King move penalties (lower king-move priority in the opening if not castling)
        if (fromPieceType == chess::Board::KING) {
            const int fileDelta = std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index));
            const bool isCastling = (fileDelta == 2);

            if (fullMoveClock < 10 && !inCheck && !isCastling) {
                score -= 500; // moderate penalty
            } else if (isCastling) {
                score += 1000; // castling bonus
            }
        }

        orderedScoredMoves.emplace_back(m, score);
        ++moveIndex; // Increment for lazy check detection threshold
    }

    orderedScoredMoves.sort();

    return orderedScoredMoves;
}

} // namespace engine
