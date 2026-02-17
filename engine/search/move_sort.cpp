#include "../engine.hpp"
#include "../tt.hpp"

namespace engine {

uint8_t Engine::getLeastValuableAttackerTo(const chess::Board& b, uint8_t sq, uint64_t occLocal, int sideLocal) const noexcept {
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
// Restituisce il guadagno netto materiale della cattura (positivo = buona, negativo = perdente)
// OPTIMIZATION: stop at the first favorable exchange for the passive side (early exit)
int64_t Engine::staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept {
    const uint8_t toSq = m.to.index;
    const uint8_t fromSq = m.from.index;

    const int sideActive = b.getActiveColor() == chess::Board::WHITE ? 0 : 1;
    const int sidePassive = sideActive ^ 1;

    // Valore del pezzo catturato inizialmente
    uint8_t capturedType = b.get(toSq) & chess::Board::MASK_PIECE_TYPE;
    if (capturedType == chess::Board::EMPTY) {
        // En passant: cattura un pedone
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

    // Simula scambio su occupazione locale
    uint64_t occ = b.getPiecesBitMap();
    occ ^= chess::Board::bitMask(fromSq); // rimuovi il pezzo che fa la prima cattura dalla sua casa

    // Il pezzo ora “in presa” sulla casa target, dopo la mossa iniziale, è il nostro attaccante iniziale.
    uint8_t capturedOnTargetType = b.get(fromSq) & chess::Board::MASK_PIECE_TYPE;

    int depth = 1;
    int side = sidePassive; // il prossimo a catturare è l'avversario

    // EARLY-EXIT: Solo per catture OVVIAMENTE perdenti (es. QxP con riconquista garantita)
    // TUNED: Soglia MOLTO conservativa (600cp) per evitare false positives da X-ray/pins
    // Threshold aumentato da 400cp a 600cp per maggiore safety
    // Skip SEE solo per casi EVIDENTEMENTE pessimi senza possibili complicazioni
    // IMPORTANTE: Early-exit può ignorare X-ray attacks e pinned pieces!
    // Esempio problematico: QxR con R difesa, ma torre bianca dietro la regina (X-ray)
    // Con threshold 400cp: 500+400=900 (skip), con 600cp: 500+600=1100>900 (calcola!)
    if (PIECE_VALUES[capturedType] + 600 < PIECE_VALUES[capturedOnTargetType]) {
        // Esempio: QxP → 100 + 600 < 900 → TRUE (skip SEE, ritorna -800) ✓ ancora skip
        // Esempio: QxN → 320 + 600 < 900 → FALSE (calcola SEE completo!) ✓ NEW: calcola
        // Esempio: QxR → 500 + 600 < 900 → FALSE (calcola SEE completo!) ✓
        // Esempio: RxP → 100 + 600 < 500 → FALSE (calcola SEE completo!) ✓
        // Esempio: NxP → 100 + 600 < 320 → FALSE (calcola SEE completo!) ✓
        return static_cast<int64_t>(PIECE_VALUES[capturedType] - PIECE_VALUES[capturedOnTargetType]);
    }

    while (depth < MAX_SEE_DEPTH) {
        // Trova l'attaccante meno prezioso verso la casella target
        uint8_t attacker = this->getLeastValuableAttackerTo(b, toSq, occ, side);
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

        // In questo ply si cattura il pezzo che era rimasto sulla casa target
        // (cioè il pezzo dell'ultimo catturante).
        gain[depth] = PIECE_VALUES[capturedOnTargetType] - gain[depth - 1];

        // Rimuovi l'attaccante dall'occupancy
        occ ^= attackerMask;

        // Ora sulla casa target rimane il pezzo che ha appena catturato: sarà lui
        // a poter essere catturato nel ply successivo.
        capturedOnTargetType = currentAttackerType;

        // Cambia lato
        side ^= 1;
        depth++;
    }

    // Negamax: propaga il miglior risultato all'indietro
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

    // Pre-calcolo variabili costose fuori dal loop
    const bool inCheck = b.inCheck(b.getActiveColor());
    const int fullMoveClock = b.getFullMoveClock();
    const int nonPawnMajors = __builtin_popcountll(
        b.knights_bb[0] | b.knights_bb[1] |
        b.bishops_bb[0] | b.bishops_bb[1] |
        b.rooks_bb[0]   | b.rooks_bb[1]   |
        b.queens_bb[0]  | b.queens_bb[1]);
    const bool isEndgameOrdering = (nonPawnMajors <= 5);

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
        for (const auto& m : moves) {
            if (m.from.index == hashFrom && m.to.index == hashTo && m.promotionPiece == hashPromo) {
                hashMoveIsLegal = true;
                break;
            }
        }
    }

    int moveIndex = 0; // Track move count for lazy check detection
    for (const auto& m : moves) {
        int64_t score = 0;

        const uint8_t fromPiece = b.get(m.from);
        const uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;

        const uint8_t toPiece = b.get(m.to);
        const uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isEpCapture = isEnPassantCapture(b, m);
        const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
        const uint8_t victimType = isEpCapture ? static_cast<uint8_t>(chess::Board::PAWN) : toPieceType;

        // =========================================================
        // MOVE ORDERING PRIORITY (from highest to lowest):
        // 1. Hash move (from TT) → 100000
        // 2. Good captures (SEE >= 0) → 10000 + MVV (1000-9000)
        // 3. Killer move 1 → 9000
        // 4. Killer move 2 → 8500
        // 5. Counter-move (response to prev move) → 8200
        // 6. Checks (non-capture, lazy: first 8 moves) → 8000
        // 7. Promotions (non-capture) → 7000
        // 8. History heuristic → 0-2000
        // 9. Bad captures (SEE < 0) → -10000 + SEE
        // =========================================================

        // Check if this is the hash move (highest priority!)
        // Only assign high priority if hash move was validated as legal
        if (hashMoveIsLegal && m.from.index == hashFrom && m.to.index == hashTo && m.promotionPiece == hashPromo) {
            score = 100000; // Highest priority
        } else if (isCapture) {
            // CAPTURES: priorità basata su SEE + capture history
            const int64_t see = staticExchangeEvaluation(b, m);
            
            if (see >= 0) {
                // GOOD CAPTURE: alta priorità (prima di killer/quiet)
                score = 10000;
                addMVVLVABonus(m, b, score); // +MVV (1000-9000)
                
                // Add capture history bonus (0-500 range)
                const int colorIndex = chess::Board::colorBoolToIndex(usIsWhite);
                const int64_t capHist = captureHistory[colorIndex][m.to.index][victimType];
                score += std::min(static_cast<int64_t>(500), capHist / 20); // Scale down
                // Total: 10000-19500
            } else {
                // BAD CAPTURE: low priority, ordered by SEE value
                // Simpler single-tier approach: all bad captures get -10000 + SEE
                score = -10000 + see;
                // Total: -10000 to -10001+ (worse SEE = lower priority)
            }
        } else {
            // NON-CAPTURES: killer, checks, history
            
            // Check for killer moves FIRST (alta priorità)
            bool isKiller = false;
            bool isCounterMove = false;
            if (ply >= 0 && ply < MAX_PLY) {
                const auto& km1 = killerMoves[0][ply];
                const auto& km2 = killerMoves[1][ply];

                if (m.from.index == km1.from.index && m.to.index == km1.to.index) {
                    score = 9000;
                    isKiller = true;
                } else if (m.from.index == km2.from.index && m.to.index == km2.to.index) {
                    score = 8500;
                    isKiller = true;
                }
            }

            // Check for counter-move (response to opponent's previous move)
            if (!isKiller && previousMove != nullptr && previousMove->from.index < 64) {
                const auto& counter = counterMoves[previousMove->from.index][previousMove->to.index];
                if (counter.from.index < 64 && m.from.index == counter.from.index && m.to.index == counter.to.index) {
                    score = 8200; // Between killer moves and checks
                    isCounterMove = true;
                }
            }

            // Se non è killer o counter-move, controlla altre eurystiche
            if (!isKiller && !isCounterMove) {
                // LAZY CHECK DETECTION: only for first 8 non-capture moves
                // Balances tactical strength with performance overhead
                if (moveIndex < 8) {
                    chess::Board::MoveState tmpState;
                    doMoveWithPromotion(b, m, tmpState);
                    const bool givesCheck = b.inCheck(b.getActiveColor());
                    b.undoMove(m, tmpState);
                    
                    if (givesCheck) {
                        score = 8000; // High priority for checking moves
                    }
                }
                
                // Promotion bonus (se non è cattura e non dà scacco già rilevato)
                if (score == 0 && fromPieceType == chess::Board::PAWN) {
                    if (m.to.rank() == chess::Board::promotionRank(usIsWhite)) {
                        score = 7000;

                        const char promo = static_cast<char>(std::tolower(static_cast<unsigned char>(m.promotionPiece)));
                        uint8_t promoType = chess::Board::QUEEN; // default if promo char is missing
                        if (promo == 'r') promoType = chess::Board::ROOK;
                        else if (promo == 'b') promoType = chess::Board::BISHOP;
                        else if (promo == 'n') promoType = chess::Board::KNIGHT;

                        // Tie-break promotions naturally: Q > R > B > N
                        score += PIECE_VALUES[promoType];
                    }
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
                
                // History heuristic (per quiet moves normali)
                if (score == 0 && ply >= 0 && ply < MAX_PLY) {
                    const int colorIndex = chess::Board::colorBoolToIndex(usIsWhite);
                    int64_t histScore = history[colorIndex][m.from.index][m.to.index];
                    // Map history to [-2000, 4000] range for better move differentiation
                    // Negative history = moves that consistently fail = ordered below neutral
                    score = std::min(static_cast<int64_t>(4000), std::max(static_cast<int64_t>(-2000), histScore));
                }

                // In endgames prioritize pawn pushes slightly, especially advanced ones.
                // This is ordering-only: it does not force pushes, but avoids searching
                // king shuffles before obvious pawn-race candidates.
                if (fromPieceType == chess::Board::PAWN && isEndgameOrdering) {
                    const int fromFile = chess::Board::fileOf(m.from.index);
                    const int toFile = chess::Board::fileOf(m.to.index);
                    if (fromFile == toFile) {
                        const int toRank = chess::Board::rankOf(m.to.index);
                        const int advancement = usIsWhite ? (6 - toRank) : (toRank - 1);
                        if (advancement > 0) {
                            score += 20 + advancement * 12;
                        }
                    }
                }

                // Discourage moving the same pawn twice in the opening: small negative ordering penalty
                // Simple heuristic: if the pawn is not on its starting rank in the opening, it's likely a second move
                if (fromPieceType == chess::Board::PAWN && !isEndgameOrdering && fullMoveClock < 8) {
                    const int fromRank = chess::Board::rankOf(m.from.index);
                    const int pawnStartRank = usIsWhite ? 6 : 1; // white pawns start on rank index 6, black on 1
                    if (fromRank != pawnStartRank) {
                        score += ORDERING_PENALTY_SAME_PAWN_OPENING; // negative value lowers priority
                    }
                }
            }
        }

        // NOTE: Stalemate check removed from move ordering (too expensive: doMove/undoMove per move!)
        // Stalemate is now handled ONLY in searchPosition() terminal node evaluation
        // This is much faster and still prevents stalemate in winning positions

        // King move penalties (riduci priorità mosse re in opening se non arrocco)
        if (fromPieceType == chess::Board::KING) {
            const int fileDelta = std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index));
            const bool isCastling = (fileDelta == 2);

            if (fullMoveClock < 10 && !inCheck && !isCastling) {
                score -= 500; // penalità moderata
            } else if (isCastling) {
                score += 1000; // bonus arrocco
            }
        }

        orderedScoredMoves.emplace_back(m, score);
        ++moveIndex; // Increment for lazy check detection threshold
    }

    orderedScoredMoves.sort();

    return orderedScoredMoves;
}

} // namespace engine
