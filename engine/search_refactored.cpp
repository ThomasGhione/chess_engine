#include "engine.hpp"

namespace engine {

constexpr int64_t CHECK_BONUS                 = 50;       // bonus per dare scacco
constexpr int64_t KILLER1_BONUS               = 100000;   // bonus killer move primaria
constexpr int64_t KILLER2_BONUS               = 90000;    // bonus killer move secondaria
constexpr int64_t KING_NON_CASTLING_PENALTY   = 20000;    // penalita' per muovere il re senza arroccare
constexpr int64_t CASTLING_BONUS              = 5000;     // piccolo bonus per arroccare

// Helper function to check if a move is a pawn promotion candidate
bool isPromotionMove(const chess::Board& board, const chess::Board::Move& move) {
    uint8_t piece = board.get(move.from);
    uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
    uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

    return (pieceType == chess::Board::PAWN) &&
           ((pieceColor == chess::Board::WHITE && move.to.rank == 7) ||
            (pieceColor == chess::Board::BLACK && move.to.rank == 0));
}

// Helper to handle terminal nodes and transposition table lookups
bool Engine::handleSearchPrelude(chess::Board& b, int64_t& depth, const AlphaBeta& bounds, int64_t& score) {
    const uint8_t activeColor = b.getActiveColor();

    // Terminal node checks: depth 0, checkmate, stalemate
    if (depth == 0 || b.isCheckmate(activeColor) || b.isStalemate(activeColor)) {
        score = this->evaluate(b);
        return true;
    }

    // Check extension: search deeper if in check
    bool inCheck = b.inCheck(activeColor);
    if (inCheck && depth > 0) depth++;

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

    for (const auto& scoredMove : orderedScoredMoves) {
        const auto& m = scoredMove.move;
        chess::Board::MoveState state;

        // Execute move (handling promotions)
        if (isPromotionMove(b, m)) {
            b.doMove(m, state, 'q');
        } else {
            b.doMove(m, state);
        }

        // Recursively evaluate position
        int64_t childDepth = std::max(static_cast<int64_t>(0), ctx.depth - 1);
        int64_t score = this->searchPosition(b, childDepth, bounds.alpha, bounds.beta, ctx.ply + 1);

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

chess::Board::Move Engine::getBestMove(std::vector<chess::Board::Move> moves, bool searchBestMoveForWhite) {
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
              << ", exact hits (approx): " << ttExactHits
              << ", cutoff hits: " << ttCutoffHits << "\n";
#endif
}

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply) {
    this->nodesSearched++;

    // Prepare search structures
    AlphaBeta bounds{alpha, beta};
    int64_t score = 0;

    // Handle terminal nodes, check extensions, and transposition table lookups
    if (this->handleSearchPrelude(b, depth, bounds, score)) {
        return score;
    }

    const uint8_t activeColor = b.getActiveColor();
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

// Generate all legal moves using bitboard representation
std::vector<chess::Board::Move> Engine::generateLegalMoves(const chess::Board& b) const {
    std::vector<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const bool isBlack = (color == chess::Board::BLACK);
    const uint64_t occ = b.getPiecesBitMap();

    // Array of piece bitboards for current side
    uint64_t pieceBitboards[] = {
        b.pawns_bb[isBlack],
        b.knights_bb[isBlack],
        b.bishops_bb[isBlack],
        b.rooks_bb[isBlack],
        b.queens_bb[isBlack],
        b.kings_bb[isBlack]
    };

    uint64_t ownOccupancy = pieceBitboards[0] | pieceBitboards[1] | pieceBitboards[2] |
                            pieceBitboards[3] | pieceBitboards[4] | pieceBitboards[5];

    // Add all valid moves from a piece to its destination squares
    auto addMovesFromSquare = [&](uint8_t from, uint64_t destinations) {
        destinations &= ~ownOccupancy;  // Remove squares with our pieces

        while (destinations) {
            uint8_t to = static_cast<uint8_t>(__builtin_ctzll(destinations));
            destinations &= (destinations - 1);

            if (b.canMoveToBB(chess::Coords{from}, chess::Coords{to})) {
                moves.push_back(chess::Board::Move{chess::Coords{from}, chess::Coords{to}});
            }
        }
    };

    // Generate moves for each piece type using a common pattern
    auto generateForPieceType = [&](uint64_t& pieces, auto getMoveMask) {
        while (pieces) {
            uint8_t from = static_cast<uint8_t>(__builtin_ctzll(pieces));
            pieces &= (pieces - 1);
            addMovesFromSquare(from, getMoveMask(from));
        }
    };

    // Generate moves for all piece types
    generateForPieceType(pieceBitboards[0], [&](uint8_t from) {  // Pawns
        return pieces::PAWN_ATTACKS[!isBlack][static_cast<int16_t>(from)] |
               pieces::getPawnForwardPushes(static_cast<int16_t>(from), !isBlack, occ);
    });

    generateForPieceType(pieceBitboards[1], [](uint8_t from) {  // Knights
        return pieces::KNIGHT_ATTACKS[static_cast<int16_t>(from)];
    });

    generateForPieceType(pieceBitboards[2], [occ](uint8_t from) {  // Bishops
        return pieces::getBishopAttacks(static_cast<int16_t>(from), occ);
    });

    generateForPieceType(pieceBitboards[3], [occ](uint8_t from) {  // Rooks
        return pieces::getRookAttacks(static_cast<int16_t>(from), occ);
    });

    generateForPieceType(pieceBitboards[4], [occ](uint8_t from) {  // Queens
        return pieces::getQueenAttacks(static_cast<int16_t>(from), occ);
    });

    generateForPieceType(pieceBitboards[5], [](uint8_t from) {  // Kings
        return pieces::KING_ATTACKS[static_cast<int16_t>(from)];
    });

    return moves;
}

// Helper to add MVV-LVA bonus for captures
void Engine::addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) {
    uint8_t toPiece = b.get(m.to);
    uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;

    if (toPieceType != chess::Board::EMPTY) {
        uint8_t fromPiece = b.get(m.from);
        uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;
        int64_t victimValue = pieceValues.at(toPieceType);
        int64_t attackerValue = pieceValues.at(fromPieceType);
        score += (victimValue * 10 - attackerValue);
    }
}

// Helper to add promotion bonus
void Engine::addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score) {
    if (pieceType == chess::Board::PAWN) {
        if ((usIsWhite && m.to.rank == 7) || (!usIsWhite && m.to.rank == 0)) {
            score += pieceValues.at(chess::Board::QUEEN);
        }
    }
}

// Helper to add check bonus
void Engine::addCheckBonus(const chess::Board::Move& m, chess::Board& b, bool usIsWhite, int64_t& score) {
    chess::Board::MoveState tmpState;
    b.doMove(const_cast<chess::Board::Move&>(m), tmpState, isPromotionMove(b, m) ? 'q' : 0);

    uint8_t opponent = usIsWhite ? chess::Board::BLACK : chess::Board::WHITE;
    if (b.inCheck(opponent)) {
        score += CHECK_BONUS;
    }
    b.undoMove(const_cast<chess::Board::Move&>(m), tmpState);
}

// Helper to add killer move and history heuristic bonuses
void Engine::addKillerAndHistoryBonus(const chess::Board::Move& m, int ply, bool usIsWhite, int64_t& score) {
    if (ply >= MAX_PLY) return;

    const auto& km1 = killerMoves[0][ply];
    const auto& km2 = killerMoves[1][ply];

    if (m.from.file == km1.from.file && m.from.rank == km1.from.rank &&
        m.to.file == km1.to.file && m.to.rank == km1.to.rank) {
        score += KILLER1_BONUS;
    } else if (m.from.file == km2.from.file && m.from.rank == km2.from.rank &&
               m.to.file == km2.to.file && m.to.rank == km2.to.rank) {
        score += KILLER2_BONUS;
    }

    int colorIndex = usIsWhite ? 0 : 1;
    int fromIndex = m.from.rank * 8 + m.from.file;
    int toIndex = m.to.rank * 8 + m.to.file;
    score += history[colorIndex][fromIndex][toIndex];
}

// Helper to add king move heuristic bonus/penalty
void Engine::addKingMoveBonus(const chess::Board::Move& m, uint8_t pieceType, int64_t& score) {
    if (pieceType != chess::Board::KING) return;

    int fileDelta = std::abs(m.to.file - m.from.file);
    if (fileDelta != 2) {
        score -= KING_NON_CASTLING_PENALTY;
    } else {
        score += CASTLING_BONUS;
    }
}

std::vector<Engine::ScoredMove> Engine::sortLegalMoves(const std::vector<chess::Board::Move>& moves, int ply, chess::Board& b, bool usIsWhite) {
    std::vector<ScoredMove> orderedScoredMoves;
    orderedScoredMoves.reserve(moves.size());

    for (const auto& m : moves) {
        int64_t score = 0;

        uint8_t fromPiece = b.get(m.from);
        uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;
        uint8_t toPiece = b.get(m.to);
        uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
        const bool isCapture = (toPieceType != chess::Board::EMPTY);

        // Calculate score components
        this->addMVVLVABonus(m, b, score);
        this->addPromotionBonus(m, fromPieceType, usIsWhite, score);
        this->addCheckBonus(m, b, usIsWhite, score);

        // Killer move and history heuristic: only for non-captures
        if (!isCapture) {
            this->addKillerAndHistoryBonus(m, ply, usIsWhite, score);
        }

        this->addKingMoveBonus(m, fromPieceType, score);

        orderedScoredMoves.push_back(ScoredMove{m, score});
    }

    // Sort moves: higher score first for white, lower score first for black
    std::sort(orderedScoredMoves.begin(), orderedScoredMoves.end(),
              [usIsWhite](const ScoredMove& a, const ScoredMove& b) {
                  return usIsWhite ? (a.score > b.score) : (a.score < b.score);
              });

    return orderedScoredMoves;
}
}; //namespace engine
