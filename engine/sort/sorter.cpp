#include "sorter.hpp"

#include "../engine.hpp"

namespace engine {

int32_t computeSeeForPicker(const chess::Board& b, const chess::Board::Move& m) noexcept {
    return Sorter::staticExchangeEvaluationPublic(b, m);
}

Sorter::CaptureInfo Sorter::classifyCapture(
        const chess::Board::Move& m, int fromPieceType, int toPieceType,
        const chess::Coords& enPassant) noexcept {
    const bool isEpCapture = chess::Coords::isInBounds(enPassant)
        && fromPieceType == chess::Board::PAWN
        && toPieceType   == chess::Board::EMPTY
        && (m.to == enPassant)
        && (chess::Board::file(m.from.index) != chess::Board::file(m.to.index));
    const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
    const int victimType = isEpCapture ? chess::Board::PAWN : toPieceType;
    return {isCapture, victimType};
}


bool Sorter::givesCheckAfterQuietMoveFast(const chess::Board& b, const chess::Board::Move& m,
        int fromPieceType, int oppKingSq, uint64_t occ) noexcept {
    const int usSide = chess::Board::colorToIndex(b.getActiveColor());
    const uint64_t fromBit = chess::Board::bitMask(m.from.index);
    const uint64_t toBit   = chess::Board::bitMask(m.to.index);
    const uint64_t occAfter = (occ & ~fromBit) | toBit;

    uint64_t pawns   = b.pawns_bb[usSide];
    uint64_t knights = b.knights_bb[usSide];
    uint64_t bishops = b.bishops_bb[usSide];
    uint64_t rooks   = b.rooks_bb[usSide];
    uint64_t queens  = b.queens_bb[usSide];
    uint64_t kings   = b.kings_bb[usSide];

    switch (fromPieceType) {
        case chess::Board::PAWN:   pawns   = (pawns   & ~fromBit) | toBit; break;
        case chess::Board::KNIGHT: knights = (knights & ~fromBit) | toBit; break;
        case chess::Board::BISHOP: bishops = (bishops & ~fromBit) | toBit; break;
        case chess::Board::ROOK:   rooks   = (rooks   & ~fromBit) | toBit; break;
        case chess::Board::QUEEN:  queens  = (queens  & ~fromBit) | toBit; break;
        case chess::Board::KING:   kings   = (kings   & ~fromBit) | toBit; break;
        default: break;
    }

    if (pieces::PAWN_ATTACKERS_TO[usSide][oppKingSq] & pawns)   return true;
    if (pieces::KNIGHT_ATTACKS[oppKingSq] & knights)             return true;
    if (pieces::KING_ATTACKS[oppKingSq] & kings)                 return true;

    const uint64_t rookQueens   = rooks   | queens;
    const uint64_t bishopQueens = bishops | queens;
    if (rookQueens   && (pieces::getRookAttacks(oppKingSq, occAfter)   & rookQueens))   return true;
    return bishopQueens && (pieces::getBishopAttacks(oppKingSq, occAfter) & bishopQueens);
}

int32_t Sorter::scoreMoveOrderingPriorityInline(const MoveOrderingContext& ctx, const chess::Board::Move& m,
        bool isCapture, int victimType,
        bool isPromotionCandidate, bool isHashMove, int fromPieceType) noexcept {

    if (isHashMove) return HASH_MOVE_SCORE;

    if (isCapture) {
        // Captures are scored provisionally as "good" here; the losing-capture
        // (SEE < 0) demotion is applied lazily by MovePicker::finalizeSee.
        int32_t score = CAPTURE_BASE_SCORE + MVV_TABLE[victimType];
        if (isPromotionCandidate) score += getPromotionValueDelta(m.promotionPiece);
        score += std::min<int32_t>(500,
            (ctx.runtime.captureHistory[ctx.usSide][m.to.index][victimType][0]
             + (ctx.runtime.captureHistory[ctx.usSide][m.to.index][victimType][1] >> 1)) / 20);
        return std::clamp<int32_t>(score, NEG_INF, POS_INF);
    }

    if (m.sameFromTo(ctx.runtime.killerMoves[0][ctx.ply])) return KILLER_1_SCORE;
    if (m.sameFromTo(ctx.runtime.killerMoves[1][ctx.ply])) return KILLER_2_SCORE;

    if (ctx.previousMove != nullptr) {
        const uint16_t counter = ctx.runtime.counterMoves[ctx.previousMove->from.index][ctx.previousMove->to.index];
        if (counter != 0
            && counter == TranspositionTable::Entry::encodeMove(m.from.index, m.to.index, m.promotionPiece)) {
            return COUNTER_MOVE_SCORE;
        }
    }

    if (isPromotionCandidate) {
        return PROMOTION_BASE_SCORE + PIECE_VALUES[promotionPieceType(m.promotionPiece)];
    }

    int32_t score = std::clamp(static_cast<int32_t>(ctx.runtime.history[ctx.usSide][m.from.index][m.to.index]),
                               HISTORY_SCORE_MIN, HISTORY_SCORE_MAX);
    if (ctx.contHistEntry != nullptr) {
        score += std::clamp(static_cast<int32_t>(ctx.contHistEntry[contHistIndex(fromPieceType, m.to.index)]),
                            HISTORY_SCORE_MIN, HISTORY_SCORE_MAX) / 2;
    }

    return std::clamp<int32_t>(score, NEG_INF, POS_INF);
}

Sorter::LeastValuableAttacker Sorter::getLeastValuableAttackerTo(
        const chess::Board& b, int sq, uint64_t occLocal, int sideLocal) noexcept {
    uint64_t mask = b.pawns_bb[sideLocal] & occLocal & pieces::PAWN_ATTACKERS_TO[sideLocal][sq];
    if (mask) return {std::countr_zero(mask), chess::Board::PAWN};

    mask = b.knights_bb[sideLocal] & occLocal & pieces::KNIGHT_ATTACKS[sq];
    if (mask) return {std::countr_zero(mask), chess::Board::KNIGHT};

    // Cache sliding attack rays — shared by bishop/queen and rook/queen lookups.
    const uint64_t bishopRays = pieces::getBishopAttacks(sq, occLocal);
    const uint64_t rookRays   = pieces::getRookAttacks(sq, occLocal);

    mask = b.bishops_bb[sideLocal] & occLocal & bishopRays;
    if (mask) return {std::countr_zero(mask), chess::Board::BISHOP};

    mask = b.rooks_bb[sideLocal] & occLocal & rookRays;
    if (mask) return {std::countr_zero(mask), chess::Board::ROOK};

    mask = b.queens_bb[sideLocal] & occLocal & (bishopRays | rookRays);
    if (mask) return {std::countr_zero(mask), chess::Board::QUEEN};

    mask = b.kings_bb[sideLocal] & occLocal & pieces::KING_ATTACKS[sq];
    return mask ? LeastValuableAttacker{std::countr_zero(mask), chess::Board::KING}
                : LeastValuableAttacker{64, 0};
}

int32_t Sorter::staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) noexcept {
    struct SEECacheEntry { 
        uint64_t key; 
        int32_t score; 
        uint8_t valid; 
    };
    static constexpr size_t SEE_CACHE_SIZE = 1u << 16;
    static constexpr uint64_t SEE_CACHE_MASK = SEE_CACHE_SIZE - 1u;
    thread_local std::array<SEECacheEntry, SEE_CACHE_SIZE> seeCache{};

    const int toSq   = m.to.index;
    const int fromSq = m.from.index;

    // Cheap key: 0 multiplies. from/to/promo land in distinct bit regions
    // (0..5, 6..11, 12..) so within the same position they disperse cleanly;
    // the zobrist hash provides cross-position dispersion in the high bits.
    const uint64_t cacheKey = b.getHash()
        ^ static_cast<uint64_t>(fromSq)
        ^ (static_cast<uint64_t>(toSq) << 6)
        ^ (static_cast<uint64_t>(static_cast<unsigned char>(m.promotionPiece)) << 12);
    SEECacheEntry& entry = seeCache[cacheKey & SEE_CACHE_MASK];
    if (entry.valid && entry.key == cacheKey) return entry.score;

    const int sideActive  = chess::Board::colorToIndex(b.getActiveColor());
    const int sidePassive = sideActive ^ 1;

    int capturedType = b.get(toSq) & chess::Board::MASK_PIECE_TYPE;
    int capturedOnTargetType = b.get(fromSq) & chess::Board::MASK_PIECE_TYPE;
    const bool isEp = (capturedOnTargetType == chess::Board::PAWN)
                   && (m.to == b.getEnPassant());
    if (isEp) capturedType = chess::Board::PAWN;

    int32_t initialGain = PIECE_VALUES[capturedType];

    if (m.promotionPiece != '\0') {
        const int promoType = promotionPieceType(m.promotionPiece);
        // Defensive: an unrecognised promo char (sentinel EMPTY) used to fall
        // through to QUEEN, applying a phantom +Q-P gain in SEE. Skip cleanly.
        if (promoType != chess::Board::EMPTY) {
            initialGain += PIECE_VALUES[promoType] - PIECE_VALUES[chess::Board::PAWN];
            capturedOnTargetType = promoType;
        }
    }

    constexpr int MAX_SEE_DEPTH = 16;
    int32_t gain[MAX_SEE_DEPTH];
    gain[0] = initialGain;

    uint64_t occ = b.getPiecesBitMap();
    occ ^= chess::Board::bitMask(fromSq);
    if (isEp) {
        const uint8_t epCapturedSq = (chess::Board::rank(fromSq) * 8) + chess::Board::file(toSq);
        occ ^= chess::Board::bitMask(epCapturedSq);
    }

    int depth = 1;
    int side  = sidePassive;

    while (depth < MAX_SEE_DEPTH) {
        const auto lva = getLeastValuableAttackerTo(b, toSq, occ, side);
        if (lva.square == 64) break;

        gain[depth] = PIECE_VALUES[capturedOnTargetType] - gain[depth - 1];
        occ ^= chess::Board::bitMask(lva.square);
        capturedOnTargetType = lva.type;
        side ^= 1;
        ++depth;
    }

    while (--depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }

    entry.key   = cacheKey;
    entry.score = gain[0];
    entry.valid = 1;
    return gain[0];
}

MovePicker Sorter::sortLegalMoves(
    MoveList moves,
    int ply,
    const chess::Board& b,
    const SearchRuntime& runtime,
    const TranspositionTable* transpositionTable,
    const chess::Board::Move* previousMove,
    bool* outHashMoveIsLegal,
    const int16_t* contHistEntry) noexcept {

    MovePicker picker;
    picker.size  = moves.size;
    picker.moves = std::move(moves);
    if (picker.moves.is_empty()) [[unlikely]] {
        if (outHashMoveIsLegal != nullptr) *outHashMoveIsLegal = false;
        return picker;
    }

    const bool usIsWhite = (b.getActiveColor() == chess::Board::WHITE);
    const int usSide  = chess::Board::colorToIndex(b.getActiveColor());
    const int promotionRank = chess::Board::promotionRank(usIsWhite);
    const chess::Coords enPassant   = b.getEnPassant();
    const int fullMoveClock  = b.getFullMoveClock();
    const bool inCheck       = b.inCheck(b.getActiveColor());

    const MoveOrderingContext orderingCtx{
        previousMove, runtime, contHistEntry, ply, usSide
    };

    // Probe TT for hash move.
    uint16_t encodedHashMove = 0;
    const bool isHashMoveProbed = transpositionTable != nullptr
        && transpositionTable->probeMove(b.getHash(), encodedHashMove);
    const auto hashMove = isHashMoveProbed
        ? TranspositionTable::Entry::decodeMove(encodedHashMove)
        : TranspositionTable::Entry::DecodedMove{64, 64, '\0'};

    bool hashMoveFound = false;

    for (int i = 0; i < picker.moves.size; ++i) {
        const auto& m = picker.moves[i];

        const int fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const int toPieceType  = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;

        const auto cap = classifyCapture(m, fromPieceType, toPieceType, enPassant);
        const bool isCapture = cap.isCapture;
        const bool isPromotionCandidate = (fromPieceType == chess::Board::PAWN) && (m.to.rank() == promotionRank);

        const bool isHashMove = isHashMoveProbed
            && m.sameFromTo(hashMove.from, hashMove.to)
            && m.promotionPiece == hashMove.promo;
        if (isHashMove) hashMoveFound = true;

        // Lazy SEE: 1 = capture, 2 = quiet that could be SEE-demoted, 0 = score is
        // already final. The actual SEE is deferred to the picker (finalizeSee) so
        // moves a beta cutoff never reaches don't pay for it.
        const SeePending pending = isHashMove ? SeePending::Final
            : (isCapture ? SeePending::Capture
              : ((!isPromotionCandidate && fromPieceType != chess::Board::KING) ? SeePending::Quiet : SeePending::Final));

        // Score provisionally: captures rank as good, quiets as their base score.
        // finalizeSee later applies the good/bad split and the hanging demotion.
        int32_t score = scoreMoveOrderingPriorityInline(
            orderingCtx, m, isCapture, cap.victimType,
            isPromotionCandidate, isHashMove, fromPieceType);

        if (fromPieceType == chess::Board::KING) {
            const int fileDelta = std::abs(chess::Board::file(m.to.index) - chess::Board::file(m.from.index));
            const bool isCastling = (fileDelta == 2);
            if (fullMoveClock < OPENING_FULLMOVE_THRESHOLD && !inCheck && !isCastling) {
                score -= OPENING_KING_MOVE_PENALTY;
            } else if (isCastling) {
                score += CASTLING_BONUS;
            }
        }

        picker.scores[i] = score;
        picker.seePending[i] = pending;
    }

    picker.board = &b;
    picker.hashMoveIsLegal = hashMoveFound;
    if (outHashMoveIsLegal != nullptr) *outHashMoveIsLegal = hashMoveFound;
    return picker;

}

MovePicker Sorter::sortTacticalMoves(
    const MoveList& tacticalMoves,
    const chess::Board& b,
    int32_t standPat,
    int32_t alpha,
    int ply) noexcept {

    MovePicker picker;
    if (tacticalMoves.is_empty()) return picker;

    const int promotionRank  = chess::Board::promotionRank((b.getActiveColor() == chess::Board::WHITE));
    const chess::Coords enPassant = b.getEnPassant();
    const int32_t seeThreshold   = (ply < 10) ? SEE_THRESHOLD_SHALLOW
                                  : (ply < 20) ? SEE_THRESHOLD_MID
                                               : SEE_THRESHOLD_DEEP;
    int filteredCount = 0;

    for (int i = 0; i < tacticalMoves.size; ++i) {
        const auto& m = tacticalMoves[i];

        const int fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
        const int toPieceType   = b.get(m.to)   & chess::Board::MASK_PIECE_TYPE;

        const auto cap = classifyCapture(m, fromPieceType, toPieceType, enPassant);
        const int victimType = cap.victimType;
        const bool isPromotion = (fromPieceType == chess::Board::PAWN) && (m.to.rank() == promotionRank);

        int32_t score;

        if (cap.isCapture) {
            int32_t capturedValue = PIECE_VALUES[victimType];
            if (isPromotion) capturedValue += getPromotionValueDelta(m.promotionPiece);

            // Delta prune: capture cannot improve standPat past alpha. Matches
            // Searcher::shouldDeltaPrune semantics (<=, fails low on equal too).
            if (standPat + capturedValue + FUTILITY_MARGIN <= alpha) continue;

            const int32_t see = staticExchangeEvaluation(b, m);
            if (see < seeThreshold) continue;
            if (standPat + see + MOVE_DELTA_MARGIN <= alpha) continue;

            score = std::clamp<int32_t>(CAPTURE_BASE_SCORE + see + MVV_TABLE[victimType], NEG_INF, POS_INF);
        } else {
            if (!isPromotion) continue;
            score = TACTICAL_PROMOTION_SCORE;
        }

        picker.moves[filteredCount]  = m;
        picker.scores[filteredCount] = score;
        // Tactical moves carry their final score (SEE already applied above);
        // mark Final so finalizeSee is a no-op. MovePicker no longer zero-inits.
        picker.seePending[filteredCount] = SeePending::Final;
        ++filteredCount;
    }

    picker.size = filteredCount;
    picker.moves.size = filteredCount;
    return picker;
}

bool Sorter::isForcingEvasion(const chess::Board& b, const chess::Board::Move& m, const chess::Coords& enPassant) noexcept {
    if ((b.get(m.to) & chess::Board::MASK_PIECE_TYPE) != chess::Board::EMPTY)
        return true;
    if ((b.get(m.from) & chess::Board::MASK_PIECE_TYPE) != chess::Board::PAWN)
        return false;
    if (m.to.rank() == chess::Board::promotionRank(b.getColor(m.from.index) == chess::Board::WHITE))
        return true;

    return chess::Coords::isInBounds(enPassant) && (m.to == enPassant);
}

MoveList Sorter::sortEvasionsForcingFirst(MoveList evasions, const chess::Board& b) noexcept {
    const auto enPassant = b.getEnPassant();

    std::ranges::stable_partition(evasions,
        [&](const auto& m) { return isForcingEvasion(b, m, enPassant);
    });

    return evasions;
}

} // namespace engine
