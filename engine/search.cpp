#include "engine.hpp"

namespace engine {
    
// Helper per LMR: verifica se la mossa è un killer move per il ply corrente
bool Engine::isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const {
    if (ply < 0 || ply >= Engine::MAX_PLY) return false;
    for (int k = 0; k < 2; ++k) {
        const auto& km = killerMoves[k][ply];
        if (m.from.index == km.from.index && m.to.index == km.to.index) {
            return true;
        }
    }
    return false;
}

// Helper function to check if a move is a pawn promotion candidate
bool isPromotionMove(const chess::Board& board, const chess::Board::Move& move) {
    uint8_t piece = board.get(move.from);
    uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
    uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

    return (pieceType == chess::Board::PAWN) &&
           ((pieceColor == chess::Board::WHITE && move.to.rank() == 7) ||
            (pieceColor == chess::Board::BLACK && move.to.rank() == 0));
}

// Helper to handle terminal nodes and transposition table lookups
bool Engine::handleSearchPrelude(chess::Board& b, int64_t& depth, const AlphaBeta& bounds, int64_t& score) {
    
    const uint8_t activeColor = b.getActiveColor();

    // NOTA: isCheckmate/isStalemate sono gestiti implicitamente quando generateLegalMoves()
    // ritorna vuoto in searchPosition, quindi non serve controllarli qui.
    if (depth <= 0) {
        score = this->evaluate(b);
        return true;
    }

    // Check extension: search deeper if in check, ma con limite per evitare esplosione
    // Limitiamo a depth massima di 10 per evitare stack overflow e segfault
    constexpr int64_t MAX_EXTENDED_DEPTH = 10;
    const bool inCheck = b.inCheck(activeColor);
    if (inCheck && depth > 0 && depth < MAX_EXTENDED_DEPTH) {
        depth++;
    }

    // Transposition table lookup
    const uint64_t hashKey = computeHashKey(b);
    if (depth >= 1) {
        prefetchTT(hashKey);
    }

    if (this->probeTTCache(hashKey, depth, bounds, score)) {
        return true;
    }

    return false;
}

// Helper to search through all moves and find best move with its score
Engine::ScoredMove Engine::searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                                       bool usIsWhite, SearchContext& ctx, AlphaBeta& bounds) {
    int64_t best = usIsWhite ? NEG_INF : POS_INF;
    chess::Board::Move bestMove = orderedScoredMoves.front().move;

    int moveIndex = 0;
    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        chess::Board::MoveState state;

        // Execute move (handling promotions)
        if (isPromotionMove(b, m)) {
            b.doMove(m, state, 'q');
        } else {
            b.doMove(m, state);
        }

        /*
        // Futility pruning: skip obviously losing captures at low depth
        // Use SEE to detect bad captures (losing material)
        const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
        const bool isCapture = (toPieceType != chess::Board::EMPTY);
        
        if (ctx.depth <= 2 && isCapture && moveIndex > 0) {
            int64_t see = staticExchangeEvaluation(b, m);
            // If we lose more than a pawn's worth of material, skip this capture at shallow depth
            if (see < -100) {
                b.undoMove(m, state);
                ++moveIndex;
                continue;
            }
        } */

        // LMR: riduci la profondità per le mosse "late" non critiche
        int64_t childDepth = std::max(static_cast<int64_t>(0), ctx.depth - 1);
        bool canReduce = (ctx.depth > 2)
            && (moveIndex > 1)
            && !isPromotionMove(b, m)
            && (b.get(m.to) == chess::Board::EMPTY) // non-capture
            && !this->isKillerMove(m, killerMoves, ctx.ply);

        int64_t score = 0;
        if (canReduce) {
            int64_t reducedDepth = std::max(static_cast<int64_t>(0), childDepth - 1);
            score = this->searchPosition(b, reducedDepth, bounds.alpha, bounds.beta, ctx.ply + 1);
            // Se la mossa ridotta taglia, ricerchi a profondità piena
            if (score > bounds.alpha && score < bounds.beta) {
                score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1);
            }
        } else {
            score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1);
        }

        // Undo move
        b.undoMove(m, state);

        // Update best score and alpha-beta bounds
        this->updateMinMax(usIsWhite, score, bounds.alpha, bounds.beta, best, bestMove, m);

        // Beta cutoff: update killer moves and history, then break
        if (bounds.alpha >= bounds.beta) {
            this->updateKillerAndHistoryOnBetaCutoff(b, m, ctx.depth, ctx.ply, ctx.activeColor,
                                                  bounds.alpha, bounds.beta, history, killerMoves);
            break;
        }
        ++moveIndex;
    }

    return ScoredMove{bestMove, best};
}

void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                          chess::Board::Move& bestMove, const chess::Board::Move& m) {
    if (usIsWhite) {
        // White is the maximizing player
        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }
        if (score > alpha) alpha = score;
        return;
    }
    // Black is the minimizing player
    if (score < bestScore) {
        bestScore = score;
        bestMove = m;
    }
    if (score < beta) beta = score;
}

void Engine::updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& best) {
    if (usIsWhite) {
        if (score > best) best = score;
        if (score > alpha) alpha = score;
        return; 
    }
    if (score < best) best = score;
    if (score < beta) beta = score;
}

chess::Board::Move Engine::getBestMove(const MoveList<chess::Board::Move>& moves, bool searchBestMoveForWhite) {
    // Alpha-beta pruning: White maximizes, Black minimizes
    int64_t alpha = this->NEG_INF;
    int64_t beta = this->POS_INF;
    int64_t bestScore = searchBestMoveForWhite ? NEG_INF : POS_INF;

    chess::Board::Move bestMove = moves.front();
    constexpr int currPly = 1;

    for (const auto& m : moves) {
        chess::Board::MoveState state;

        this->executeMove(m, state);
        int64_t score = this->searchPosition(this->board, this->depth - 1, alpha, beta, currPly);
        
        this->undoAndUpdateMove(m, state, searchBestMoveForWhite, score, alpha, beta, bestScore, bestMove);
    }

    this->eval = bestScore;
    return bestMove;
}

// Helper to execute a move, handling promotions
void Engine::executeMove(const chess::Board::Move& m, chess::Board::MoveState& state) {
    if (isPromotionMove(this->board, m)) {
        this->board.doMove(const_cast<chess::Board::Move&>(m), state, 'q');
        return;
    }
    this->board.doMove(const_cast<chess::Board::Move&>(m), state);
}

// Helper to undo move and update best move/alpha-beta bounds
void Engine::undoAndUpdateMove(const chess::Board::Move& m, chess::Board::MoveState& state, bool usIsWhite,
                               int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore,
                               chess::Board::Move& bestMove) {
    // Undo move before processing next one
    this->board.undoMove(const_cast<chess::Board::Move&>(m), state);

    // Update best move and alpha-beta bounds
    this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);
}

void Engine::doMoveInBoard(chess::Board::Move bestMove) {
    // Execute the best move found, handling promotions
    if (isPromotionMove(this->board, bestMove)) {
        (void)this->board.moveBB(bestMove.from, bestMove.to, 'q');
        return;
    }
    (void)this->board.moveBB(bestMove.from, bestMove.to);
}

void Engine::search(uint64_t depth) {
    if (depth == 0) return;

    MoveList<chess::Board::Move> moves = this->generateLegalMoves(this->board);
    if (moves.is_empty()) {
        this->eval = this->evaluate(this->board);
        return;
    }

    // Reset the nodes searched counter
    this->nodesSearched = 0; 

    const bool searchBestMoveForWhite = (this->board.getActiveColor() == chess::Board::WHITE);
    chess::Board::Move bestMove = this->getBestMove(moves, searchBestMoveForWhite);

    this->doMoveInBoard(bestMove);

#ifdef DEBUG
/*
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    std::cout << "Engine plays: " << moveStr << " (score: " << this->eval << ")\n";

    std::cout << "[DEBUG] TT probes: " << ttProbes
              << ", hits: " << ttHits
              << "\n";
*/
#endif
}

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply) {
    this->nodesSearched++;

    // SAFETY CHECK: evita stack overflow e accesso fuori bounds a killerMoves/history
    if (ply >= MAX_PLY - 1) {
        return this->evaluate(b);
    }

    // Prepare search structures
    AlphaBeta bounds{alpha, beta};
    int64_t score = 0;

    // Handle terminal nodes, check extensions, and transposition table lookups
    if (this->handleSearchPrelude(b, depth, bounds, score)) {
        return score;
    }

    const uint8_t activeColor = b.getActiveColor();

    // --- Null Move Pruning DISABILITATO ---
    // Reintrodurre quando avremo: hash move, better move ordering, e tactical position detection
    // Il problema: senza hash move, taglia anche rami con catture ovvie
    /*
    auto hasNonPawnMaterial = [&]() {
        const int side = (activeColor == chess::Board::WHITE) ? 0 : 1;
        int material = __builtin_popcountll(b.knights_bb[side]) +
                       __builtin_popcountll(b.bishops_bb[side]) +
                       __builtin_popcountll(b.rooks_bb[side]) +
                       __builtin_popcountll(b.queens_bb[side]);
        return material > 0;
    };

    if (depth >= 3 && !b.inCheck(activeColor) && hasNonPawnMaterial() && ply >= 1) {
        const int R = (depth >= 6) ? 3 : 2;
        b.setNextTurn();
        int64_t nullScore = -this->searchPosition(b, depth - 1 - R, -beta, -alpha, ply + 1);
        b.setPrevTurn();
        if (nullScore >= beta) {
            return beta;
        }
    }
    */


    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    MoveList<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.is_empty()) return this->evaluate(b);

    MoveList<ScoredMove> orderedScoredMoves = this->sortLegalMoves(moves, ply, b, usIsWhite);
    const int64_t alphaOrig = bounds.alpha;

    // Build search context
    SearchContext ctx{depth, bounds.alpha, bounds.beta, ply, activeColor};

    // Search through all moves and find best move with score
    ScoredMove result = this->searchMoves(b, orderedScoredMoves, usIsWhite, ctx, bounds);
    int64_t best = result.score;

    // Save position to transposition table
    const uint64_t hashKey = computeHashKey(b);
    TTSaveInfo ttInfo{hashKey, depth, best, alphaOrig, bounds.beta};
    this->saveTTEntry(ttInfo);
    return best;
}

__attribute__((always_inline))
inline void addMovesFromMask_fast(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    const uint8_t from,
    uint64_t mask,
    const uint64_t ownOcc,
    const bool inCheck
) {
    mask &= ~ownOcc;

    const chess::Coords fromC{from};

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        chess::Coords toC{to};

        if (b.canMoveToBB(fromC, toC, inCheck)) [[likely]] {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}

MoveList<chess::Board::Move>
Engine::generateLegalMoves(const chess::Board& b) const 
{
    MoveList<chess::Board::Move> moves;
    // moves.reserve(40);

    const uint8_t color    = b.getActiveColor();
    const int side         = (color == chess::Board::WHITE) ? 0 : 1;
    const uint8_t oppColor = (color == chess::Board::WHITE) ? chess::Board::BLACK : chess::Board::WHITE;

    const uint64_t occ     = b.getPiecesBitMap();

    const uint64_t pawns   = b.pawns_bb[side];
    const uint64_t knights = b.knights_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    const uint64_t rooks   = b.rooks_bb[side];
    const uint64_t queens  = b.queens_bb[side];
    const uint64_t kings   = b.kings_bb[side];

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;

    // Pre-calculate inCheck once for all moves
    const bool inCheck = b.inCheck(color);
    
    // For pawn move generation
    const bool isWhite = (side == 0);



    // --------------------------------------------------------------------
    // 1) KING
    // --------------------------------------------------------------------
    if (kings) [[likely]] {

        const uint8_t kingIndex = __builtin_ctzll(kings);
        const chess::Coords kingPos{kingIndex};
        uint64_t kmask = pieces::KING_ATTACKS[kingIndex] & ~ownOcc;

        while (kmask) {
            const uint8_t to = __builtin_ctzll(kmask);
            kmask &= (kmask - 1);

            if (!b.isSquareAttacked(to, oppColor, kingIndex)) {
                moves.emplace_back(chess::Board::Move{
                    kingPos, chess::Coords{to}
                });
            }
        }

        // Castling: usa direttamente la posizione attuale del re
        {
            // lato bianco: re deve essere su e1; lato nero: su e8
            // ma questo controllo lo fa già canMoveToBB guardando fromIndex/rank/file.
            const uint8_t kingFile = kingIndex & 7;
            const uint8_t toRank   = kingIndex >> 3;
        
            // Tentativo arrocco corto: file + 2
            if (kingFile <= 5) {
                const uint8_t toKsIndex = kingIndex + 2;
                chess::Coords toKs{toKsIndex};
                if (b.canMoveToBB(kingPos, toKs, inCheck)) {
                    moves.emplace_back(chess::Board::Move{kingPos, toKs});
                }
            }
        
            // Tentativo arrocco lungo: file - 2
            if (kingFile >= 2) {
                const uint8_t toQsIndex = kingIndex - 2;
                chess::Coords toQs{toQsIndex};
                if (b.canMoveToBB(kingPos, toQs, inCheck)) {
                    moves.emplace_back(chess::Board::Move{kingPos, toQs});
                }
            }
        }
    }



    // --------------------------------------------------------------------
    // 2) PAWNS
    // --------------------------------------------------------------------
    {
        uint64_t bb = pawns;
        while (bb) {
            const uint8_t from = __builtin_ctzll(bb);
            bb &= (bb - 1);

            uint64_t mask =
                pieces::PAWN_ATTACKS[isWhite][from] |
                pieces::getPawnForwardPushes(from, isWhite, occ);

            addMovesFromMask_fast(b, moves, from, mask, ownOcc, inCheck);
        }
    }


    // --------------------------------------------------------------------
    // 3) KNIGHTS
    // --------------------------------------------------------------------
    {
        uint64_t bb = knights;
        while (bb) {
            const uint8_t from = __builtin_ctzll(bb);
            bb &= (bb - 1);

            addMovesFromMask_fast(b, moves, from,
                pieces::KNIGHT_ATTACKS[from], ownOcc, inCheck);
        }
    }


    // --------------------------------------------------------------------
    // 4) BISHOPS
    // --------------------------------------------------------------------
    {
        uint64_t bb = bishops;
        while (bb) {
            const uint8_t from = __builtin_ctzll(bb);
            bb &= (bb - 1);

            addMovesFromMask_fast(
                b, moves, from,
                pieces::getBishopAttacks(from, occ),
                ownOcc,
                inCheck
            );
        }
    }


    // --------------------------------------------------------------------
    // 5) ROOKS
    // --------------------------------------------------------------------
    {
        uint64_t bb = rooks;
        while (bb) {
            const uint8_t from = __builtin_ctzll(bb);
            bb &= (bb - 1);

            addMovesFromMask_fast(
                b, moves, from,
                pieces::getRookAttacks(from, occ),
                ownOcc,
                inCheck
            );
        }
    }


    // --------------------------------------------------------------------
    // 6) QUEENS
    // --------------------------------------------------------------------
    {
        uint64_t bb = queens;
        while (bb) {
            const uint8_t from = __builtin_ctzll(bb);
            bb &= (bb - 1);

            addMovesFromMask_fast(
                b, moves, from,
                pieces::getQueenAttacks(from, occ),
                ownOcc,
                inCheck
            );
        }
    }

    return moves;
}

// Helper to add MVV-LVA bonus for captures
void Engine::addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) {

    const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
    const uint8_t toPieceType   = b.get(m.to)   & chess::Board::MASK_PIECE_TYPE;

    if (toPieceType != chess::Board::EMPTY) [[likely]] {
        score += MVV_LVA_TABLE[toPieceType][fromPieceType];
        return;
    }

    // En passant (only pawn moving diagonally to empty square)
    if (fromPieceType == chess::Board::PAWN) [[unlikely]] {
        if ((m.from.index & 7) != (m.to.index & 7)) {
            score += MVV_LVA_TABLE[chess::Board::PAWN][chess::Board::PAWN];
        }
    }
}


// Helper to add promotion bonus
void Engine::addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score) {
    if (pieceType == chess::Board::PAWN) {
        if ((usIsWhite && m.to.rank() == 7) || (!usIsWhite && m.to.rank() == 0)) {
            score += PIECE_VALUES[chess::Board::QUEEN];
        }
    }
}

// Helper to add check bonus
void Engine::addCheckBonus(const chess::Board::Move& m, chess::Board& b, bool usIsWhite, int64_t& score) {
    chess::Board::MoveState tmpState;
    b.doMove(const_cast<chess::Board::Move&>(m), tmpState, isPromotionMove(b, m) ? 'q' : 0);
    if (b.inCheck(!usIsWhite)) {
        score += CHECK_BONUS;
    }
    b.undoMove(const_cast<chess::Board::Move&>(m), tmpState);
}

// Helper to add killer move and history heuristic bonuses
void Engine::addKillerAndHistoryBonus(const chess::Board::Move& m, int ply, bool usIsWhite, int64_t& score) {
    if (ply >= MAX_PLY) return;

    const auto& km1 = killerMoves[0][ply];
    const auto& km2 = killerMoves[1][ply];

    if (m.from.index == km1.from.index && m.to.index == km1.to.index) {
        score += KILLER1_BONUS;
    } else if (m.from.index == km2.from.index && m.to.index == km2.to.index) {
        score += KILLER2_BONUS;
    }

    int colorIndex = usIsWhite ? 0 : 1;
    int fromIndex = m.from.index;
    int toIndex = m.to.index;
    score += history[colorIndex][fromIndex][toIndex];
}

// Helper to add king move heuristic bonus/penalty
// NOTA: inCheck precalcolato fuori dal loop per evitare chiamate ripetute
void Engine::addKingMoveBonus(const chess::Board::Move& m, uint8_t pieceType, bool inCheck, int fullMoveClock, int64_t& score) {
    if (pieceType != chess::Board::KING) return;

    const int fileDelta = std::abs((m.to.index & 7) - (m.from.index & 7));
    const bool isCastling = (fileDelta == 2);

    // Penalizza mosse del re in apertura se non sotto scacco e non arrocco
    if (fullMoveClock < 10 && !inCheck && !isCastling) {
        score -= KING_NON_CASTLING_PENALTY;
    } else if (isCastling) { 
        score += CASTLING_BONUS;
    }
}

// Static Exchange Evaluation (SEE)
// Restituisce il guadagno netto materiale della cattura (positivo = buona, negativo = perdente)
int64_t Engine::staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const {
    const uint8_t toSq = m.to.index;
    const uint8_t fromSq = m.from.index;

    // Valori dei pezzi (EMPTY=0, PAWN=1, ..., KING=6)
    constexpr int64_t pieceVals[7] = {0, 100, 320, 330, 500, 900, 20000};

    // Trova tutti gli attaccanti alla casella target
    auto getAttackersTo = [&](uint8_t sq, uint64_t occ, int side) -> uint64_t {
        uint64_t attackers = 0ULL;
        
        // Pawns
        attackers |= b.pawns_bb[side] & pieces::PAWN_ATTACKERS_TO[side][sq];
        
        // Knights
        attackers |= b.knights_bb[side] & pieces::KNIGHT_ATTACKS[sq];
        
        // Kings
        attackers |= b.kings_bb[side] & pieces::KING_ATTACKS[sq];
        
        // Bishops and Queens (diagonal)
        uint64_t diagSliders = b.bishops_bb[side] | b.queens_bb[side];
        if (diagSliders) {
            uint64_t bishopAtks = pieces::getBishopAttacks(sq, occ);
            attackers |= diagSliders & bishopAtks;
        }
        
        // Rooks and Queens (orthogonal)
        uint64_t orthSliders = b.rooks_bb[side] | b.queens_bb[side];
        if (orthSliders) {
            uint64_t rookAtks = pieces::getRookAttacks(sq, occ);
            attackers |= orthSliders & rookAtks;
        }
        
        return attackers;
    };

    // Trova l'attaccante meno prezioso
    auto getLeastValuableAttacker = [&](uint64_t attackers, const chess::Board& board) -> uint8_t {
        // Ordine: pawn, knight, bishop, rook, queen, king
        const uint64_t pieceTypes[6] = {
            attackers & board.pawns_bb[0] | board.pawns_bb[1],
            attackers & board.knights_bb[0] | board.knights_bb[1],
            attackers & board.bishops_bb[0] | board.bishops_bb[1],
            attackers & board.rooks_bb[0] | board.rooks_bb[1],
            attackers & board.queens_bb[0] | board.queens_bb[1],
            attackers & board.kings_bb[0] | board.kings_bb[1]
        };
        
        for (uint64_t bb : pieceTypes) {
            if (bb) return __builtin_ctzll(bb);
        }
        return 64; // nessun attaccante
    };

    const int sideActive = b.getActiveColor() == chess::Board::WHITE ? 0 : 1;
    const int sidePassive = sideActive ^ 1;

    // Valore del pezzo catturato inizialmente
    uint8_t capturedType = b.get(toSq) & chess::Board::MASK_PIECE_TYPE;
    if (capturedType == chess::Board::EMPTY) {
        // En passant: cattura un pedone
        capturedType = chess::Board::PAWN;
    }

    int64_t gain[32];
    gain[0] = pieceVals[capturedType];

    // Simula scambio
    uint64_t occ = b.getPiecesBitMap();
    uint64_t fromBB = 1ULL << fromSq;
    occ ^= fromBB; // rimuovi il pezzo che fa la prima cattura
    
    uint8_t attackerType = b.get(fromSq) & chess::Board::MASK_PIECE_TYPE;
    int depth = 1;
    int side = sidePassive; // il prossimo a catturare è l'avversario

    while (depth < 32) {
        // Trova attaccanti del lato corrente
        uint64_t attackers = getAttackersTo(toSq, occ, side);
        if (!attackers) break;

        // Prendi l'attaccante meno prezioso
        uint8_t attacker = getLeastValuableAttacker(attackers, b);
        if (attacker == 64) break;

        // Calcola guadagno: catturi il pezzo precedente, perdi quello corrente
        gain[depth] = pieceVals[attackerType] - gain[depth - 1];
        
        // Rimuovi l'attaccante dall'occupancy
        occ ^= (1ULL << attacker);
        attackerType = b.get(attacker) & chess::Board::MASK_PIECE_TYPE;
        
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
    bool usIsWhite)
{
    // const size_t n = moves.size();
    MoveList<ScoredMove> orderedScoredMoves;
    // orderedScoredMoves.reserve(n); // pre-allocazione

    // Pre-calcolo variabili costose fuori dal loop
    const bool inCheck = b.inCheck(b.getActiveColor());
    const int fullMoveClock = b.getFullMoveClock();

    // Cache alcune maschere per pezzi (tipo & tipo pezzo) per evitare doppie chiamate
    // orderedScoredMoves.reserve(n);

    for (size_t i = 0; i < moves.size; ++i) {
        const auto& m = moves[i]; // const reference per evitare copia
        int64_t score = 0;

        const uint8_t fromPiece = b.get(m.from);
        const uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;

        const uint8_t toPiece = b.get(m.to);
        const uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isCapture = (toPieceType != chess::Board::EMPTY);

        // Add MVV-LVA bonus for captures (including en passant)
        addMVVLVABonus(m, b, score);
        
        // Add promotion bonus
        addPromotionBonus(m, fromPieceType, usIsWhite, score);

        // For non-captures: use killer moves and history heuristic
        if (!isCapture) {
            addKillerAndHistoryBonus(m, ply, usIsWhite, score);
        }

        // King move penalties/bonuses (castling, early king moves)
        addKingMoveBonus(m, fromPieceType, inCheck, fullMoveClock, score);

        // Emplace direttamente senza copia aggiuntiva
        orderedScoredMoves.emplace_back(m, score);
    }

    // Ordina usando lambda inline e [niente copie]
    //std::sort(orderedScoredMoves.begin(), orderedScoredMoves.end(),
    //    [](const ScoredMove& a, const ScoredMove& b) noexcept {
    //        return a.score > b.score;
    //});
    orderedScoredMoves.sort();

    // Debug: verifica che l'ordinamento sia corretto (decrescente)
    #ifdef DEBUG
    //for (size_t i = 1; i < orderedScoredMoves.size; ++i) {
    //    if (orderedScoredMoves[i-1].score < orderedScoredMoves[i].score) {
    //        std::cerr << "SORT BUG: at index " << i 
    //                  << ", score[" << (i-1) << "]=" << orderedScoredMoves[i-1].score
    //                  << " < score[" << i << "]=" << orderedScoredMoves[i].score << "\n";
    //    }
    //}
    #endif

    return orderedScoredMoves;
}


}; //namespace engine
