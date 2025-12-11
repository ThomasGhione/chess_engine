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
Engine::ScoredMove Engine::searchMoves(chess::Board& b, const std::vector<ScoredMove>& orderedScoredMoves,
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

chess::Board::Move Engine::getBestMove(const std::vector<chess::Board::Move>& moves, bool searchBestMoveForWhite) {
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

    std::vector<chess::Board::Move> moves = this->generateLegalMoves(this->board);
    if (moves.empty()) {
        this->eval = this->evaluate(this->board);
        return;
    }

    // Reset the nodes searched counter
    this->nodesSearched = 0; 

    const bool searchBestMoveForWhite = (this->board.getActiveColor() == chess::Board::WHITE);
    chess::Board::Move bestMove = this->getBestMove(moves, searchBestMoveForWhite);

    this->doMoveInBoard(bestMove);

#ifdef DEBUG
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    std::cout << "Engine plays: " << moveStr << " (score: " << this->eval << ")\n";

    std::cout << "[DEBUG] TT probes: " << ttProbes
              << ", hits: " << ttHits
              << "\n";
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
    std::vector<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.empty()) return this->evaluate(b);

    std::vector<ScoredMove> orderedScoredMoves = this->sortLegalMoves(moves, ply, b, usIsWhite);
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
    std::vector<chess::Board::Move>& moves,
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

std::vector<chess::Board::Move>
Engine::generateLegalMoves(const chess::Board& b) const 
{
    std::vector<chess::Board::Move> moves;
    moves.reserve(40);

    const uint8_t color    = b.getActiveColor();
    const bool    isBlack  = (color == chess::Board::BLACK);
    const uint8_t oppColor = isBlack ? chess::Board::WHITE : chess::Board::BLACK;

    const uint64_t occ     = b.getPiecesBitMap();

    const uint64_t pawns   = b.pawns_bb[isBlack];
    const uint64_t knights = b.knights_bb[isBlack];
    const uint64_t bishops = b.bishops_bb[isBlack];
    const uint64_t rooks   = b.rooks_bb[isBlack];
    const uint64_t queens  = b.queens_bb[isBlack];
    const uint64_t kings   = b.kings_bb[isBlack];

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;

    // Pre-calculate inCheck once for all moves
    const bool inCheck = b.inCheck(color);



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

        // Castling
        const uint8_t expected = isBlack ? 4 : 60;

        if (kingIndex == expected) {
            // Ks
            chess::Coords toKs{uint8_t(expected + 2)};
            if (b.canMoveToBB(kingPos, toKs, inCheck))
                moves.emplace_back(chess::Board::Move{kingPos, toKs});

            // Qs
            chess::Coords toQs{uint8_t(expected - 2)};
            if (b.canMoveToBB(kingPos, toQs, inCheck))
                moves.emplace_back(chess::Board::Move{kingPos, toQs});
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
                pieces::PAWN_ATTACKS[!isBlack][from] |
                pieces::getPawnForwardPushes(from, !isBlack, occ);

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
    uint8_t toPiece = b.get(m.to);
    uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;

    if (toPieceType != chess::Board::EMPTY) {
        uint8_t fromPiece = b.get(m.from);
        uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;
        int64_t victimValue = PIECE_VALUES[toPieceType];
        int64_t attackerValue = PIECE_VALUES[fromPieceType];
        score += (victimValue * 10 - attackerValue);
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

std::vector<Engine::ScoredMove> Engine::sortLegalMoves(
    const std::vector<chess::Board::Move>& moves,
    int ply,
    chess::Board& b,
    bool usIsWhite)
{
    const size_t n = moves.size();
    std::vector<ScoredMove> orderedScoredMoves;
    orderedScoredMoves.reserve(n); // pre-allocazione

    // Pre-calcolo variabili costose fuori dal loop
    const bool inCheck = b.inCheck(b.getActiveColor());
    const int fullMoveClock = b.getFullMoveClock();

    // Cache alcune maschere per pezzi (tipo & tipo pezzo) per evitare doppie chiamate
    orderedScoredMoves.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        const auto& m = moves[i]; // const reference per evitare copia
        int64_t score = 0;

        const uint8_t fromPiece = b.get(m.from);
        const uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;

        const uint8_t toPiece = b.get(m.to);
        const uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isCapture = (toPieceType != chess::Board::EMPTY);

        // Add bonus
        addMVVLVABonus(m, b, score);                  // catture
        addPromotionBonus(m, fromPieceType, usIsWhite, score); // promozioni

        if (!isCapture) {
            addKillerAndHistoryBonus(m, ply, usIsWhite, score); // killer/history
        }

        addKingMoveBonus(m, fromPieceType, inCheck, fullMoveClock, score);

        // Emplace direttamente senza copia aggiuntiva
        orderedScoredMoves.emplace_back(m, score);
    }

    // Ordina usando lambda inline e [niente copie]
    std::sort(orderedScoredMoves.begin(), orderedScoredMoves.end(),
        [](const ScoredMove& a, const ScoredMove& b) noexcept {
            return a.score > b.score;
        });

    return orderedScoredMoves;
}


}; //namespace engine
