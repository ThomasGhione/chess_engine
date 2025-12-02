#include "engine.hpp"

namespace engine {

constexpr int64_t CHECK_BONUS                 = 50;       // bonus per dare scacco
constexpr int64_t KILLER1_BONUS               = 100000;   // bonus killer move primaria
constexpr int64_t KILLER2_BONUS               = 90000;    // bonus killer move secondaria
constexpr int64_t KING_NON_CASTLING_PENALTY   = 20000;    // penalita' per muovere il re senza arroccare
constexpr int64_t CASTLING_BONUS              = 5000;     // piccolo bonus per arroccare


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

chess::Board::Move Engine::getBestMove(std::vector<chess::Board::Move> moves, bool searchBestMoveForWhite){
  // Alpha-beta is always from the side-to-move point of view:
  // White tries to maximise the score, Black to minimise it.
  int64_t alpha = this->NEG_INF;
  int64_t beta  = this->POS_INF;
  int64_t bestScore = (searchBestMoveForWhite) ? NEG_INF : POS_INF;

  chess::Board::Move bestMove = moves.front(); // temporary initialization
  for (const auto& m : moves) {
    chess::Board::MoveState state;

    uint8_t piece = this->board.get(m.from);
    uint8_t pieceType  = piece & chess::Board::MASK_PIECE_TYPE;
    uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

    bool moveOk = false;

    const bool isPromotionCandidate =
        (pieceType == chess::Board::PAWN) &&
        ((pieceColor == chess::Board::WHITE && m.to.rank == 7) ||
          (pieceColor == chess::Board::BLACK && m.to.rank == 0));

    if (isPromotionCandidate) {
        // Engine always promotes to queen for now
        this->board.doMove(m, state, 'q');
        moveOk = true; // doMove non fa legality check; generateLegalMoves garantisce pseudo-legalità
    } else {
        this->board.doMove(m, state);
        moveOk = true;
    }

    if (!moveOk) {
        continue;
    }
    constexpr int currPly = 1;
    int64_t score = this->searchPosition(this->board, this->depth - 1, alpha, beta, currPly);

    // Ripristina la board allo stato precedente
    this->board.undoMove(m, state);

    this->updateMinMax(searchBestMoveForWhite, score, alpha, beta, bestScore, bestMove, m);
  }

  // Debug update info
  this->eval = bestScore;

  return bestMove;
}

void Engine::doMoveInBoard(chess::Board::Move bestMove){
  // Esegui sulla board principale la mossa migliore trovata (gestendo promozioni)

  uint8_t piece = this->board.get(bestMove.from);
  uint8_t pieceType  = piece & chess::Board::MASK_PIECE_TYPE;
  uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

  const bool isPromotionCandidate =
      (pieceType == chess::Board::PAWN) &&
      ((pieceColor == chess::Board::WHITE && bestMove.to.rank == 7) ||
        (pieceColor == chess::Board::BLACK && bestMove.to.rank == 0));

  if (isPromotionCandidate) {
      (void)this->board.moveBB(bestMove.from, bestMove.to, 'q');
  } else {
      (void)this->board.moveBB(bestMove.from, bestMove.to);
  }
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
/*
bool Engine::hasSearchStop(int64_t& depth, chess::Board& b, int64_t& evaluate){
  const uint8_t activeColor = b.getActiveColor();
  if (depth == 0 || b.isCheckmate(activeColor) || b.isStalemate(activeColor)) {
      evaluate = this->evaluate(b);
      return true;
  }

  bool inCheck = b.inCheck(activeColor);
  if (inCheck && depth > 0) depth++; // extend search if in check

  // Prova a usare la transposition table solo per depth >= 2
  const uint64_t hashKey = computeHashKey(b);
  if (depth >= 2) {
    evaluate = static_cast<int64_t>(ttScore);
    return true;
  }

  return false;
}

std::vector<engine::Engine::ScoredMove> Engine::getOrderedScoreMoveForCurrentPosition(chess::Board& b, int ply){
  const bool searchBestMoveForWhite = (b.getActiveColor() == chess::Board::WHITE);

  std::vector<chess::Board::Move> moves = this->generateLegalMoves(b);

  return this->sortLegalMoves(moves, ply, b, searchBestMoveForWhite);
}

void Engine::savePositionToTT(int64_t best, ){
    if (depth < 2) return;


    uint8_t flag = TTEntry::EXACT;
    if (best <= alphaOrig) {
        flag = TTEntry::UPPERBOUND;   // fail-low
    } else if (best >= beta) {
        flag = TTEntry::LOWERBOUND;   // fail-high
    }

    const int32_t storedScore = static_cast<int32_t>(
        std::max<int64_t>(
            std::min<int64_t>(best, std::numeric_limits<int32_t>::max() - 1),
            std::numeric_limits<int32_t>::min() + 1));

    storeTTEntry(this->ttTable,
                 hashKey,
                 static_cast<uint16_t>(depth),
                 storedScore,
                 flag);
}

int64_t Engine::cleanSearchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply) {
  // Una posizione visitata
  this->nodesSearched++;  

  int64_t evaluateBestMove;
  if(this->hasSearchStop(depth, b, evaluateBestMove)) return evaluateBestMove;

  std::vector<ScoredMove> orderedScoredMoves = this->getOrderedScoreMoveForCurrentPosition(b);
  if (orderedScoredMoves.empty()) return this->evaluate(b);


  this->savePositionToTT();

  return evaluateBestMove;
}*/
  
int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply) {
    // Una posizione visitata
    this->nodesSearched++;  

    const uint8_t activeColor = b.getActiveColor();

    if (depth == 0) {
        return this->evaluate(b);
    }

    if (b.isCheckmate(activeColor) || b.isStalemate(activeColor)) {
        return this->evaluate(b);
    }

    bool inCheck = b.inCheck(activeColor);
    if (inCheck && depth > 0) depth++; // extend search if in check

    // Prova a usare la transposition table solo per depth >= 2
    const uint64_t hashKey = computeHashKey(b);
    if (depth >= 1) {
#ifdef DEBUG
        ++ttProbes;
#endif
        const int32_t alpha32 = static_cast<int32_t>( 
            std::max<int64_t>(alpha - TTEntry::ADJUSTMENT, NEG_INF_32 + 1));
        const int32_t beta32  = static_cast<int32_t>(
            std::min<int64_t>(beta + TTEntry::ADJUSTMENT,  POS_INF_32 - 1));
        int32_t ttScore = 0;
        if (probeTT(this->ttTable, hashKey, static_cast<uint16_t>(depth), alpha32, beta32, ttScore)) {
#ifdef DEBUG
            ++ttHits;
            ++ttExactHits;   // approssimazione: contiamo tutti gli hit come exact
            ++ttCutoffHits;  // per questo nodo, l'hit TT evita di cercare i figli
#endif
            return static_cast<int64_t>(ttScore);
        }
    }

    const bool usIsWhite = (activeColor == chess::Board::WHITE);
    std::vector<chess::Board::Move> moves = this->generateLegalMoves(b);
    if (moves.empty()) return this->evaluate(b);
    std::vector<ScoredMove> orderedScoredMoves = this->sortLegalMoves(moves, ply, b, usIsWhite);


    int64_t best = usIsWhite ? NEG_INF : POS_INF;
    const int64_t alphaOrig = alpha;

    const int totalMoves = static_cast<int>(orderedScoredMoves.size());

  int moveIndex = 0;

  // #pragma omp parallel for schedule(dynamic) // TODO check whether schedule(dynamic) works or not
  for (const auto& scoredMove : orderedScoredMoves) {

      ++moveIndex;

      const auto& m = scoredMove.move;

      // Late move pruning adattivo sulla coda delle mosse
      // if (ply >= 3 && this->shouldPruneLateMove(b, m, depth, inCheck, usIsWhite, moveIndex, totalMoves)) {
      //     continue;
      // }

      // Applica la mossa in-place con doMove/undoMove
      chess::Board::MoveState state;

      uint8_t piece = b.get(m.from);
      uint8_t pieceType  = piece & chess::Board::MASK_PIECE_TYPE;
      uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

      const bool isPromotionCandidate =
          (pieceType == chess::Board::PAWN) &&
          ((pieceColor == chess::Board::WHITE && m.to.rank == 7) ||
            (pieceColor == chess::Board::BLACK && m.to.rank == 0));

      if (isPromotionCandidate) {
          b.doMove(m, state, 'q');
      } else {
          b.doMove(m, state);
      }

      int64_t childDepth = depth - 1;
      if (childDepth < 0) childDepth = 0;

      int64_t score = this->searchPosition(b, childDepth, alpha, beta, ply + 1);
      
      this->updateMinMax(usIsWhite, score, alpha, beta, best);
      
      // Annulla la mossa prima di passare alla successiva
      b.undoMove(m, state);
      
      // TODO wrap in a function
      // Beta cutoff: update killer moves and history for non-captures
      if (alpha >= beta) {
          this->updateKillerAndHistoryOnBetaCutoff(b, m, depth, ply, activeColor,
                                              alpha, beta,
                                              history, killerMoves);
          break; // alpha-beta cutoff
      }
      
  }

    // Determina il tipo di nodo per la TT e salva 
    // per ora salva SEMPRE, e' testato che e' meglio salvare sempre rispetto a depth >= 2
    // test del 12/01/2025
    uint8_t flag = TTEntry::EXACT;
    if (best <= alphaOrig) {
        flag = TTEntry::UPPERBOUND;   // fail-low
    } else if (best >= beta) {
        flag = TTEntry::LOWERBOUND;   // fail-high
    }

    const int32_t storedScore = static_cast<int32_t>(
        std::max<int64_t>(
            std::min<int64_t>(best, std::numeric_limits<int32_t>::max() - 1),
            std::numeric_limits<int32_t>::min() + 1));

    storeTTEntry(this->ttTable,
                hashKey,
                static_cast<uint16_t>(depth),
                storedScore,
                flag);
    

    return best;
}

// Nuova versione bitboard-based per tipo
// TODO fare un array di uint64_t ownPieces per fare un for sull'array per togliere codice duplicato + parallel for
std::vector<chess::Board::Move> Engine::generateLegalMoves(const chess::Board& b) const {
    std::vector<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const bool isBlack = (color == chess::Board::BLACK);
    // Usa occupancy e bitboard per tipo/colore dalla Board
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t ownPawns   = b.pawns_bb[isBlack];
    uint64_t ownKnights = b.knights_bb[isBlack];
    uint64_t ownBishops = b.bishops_bb[isBlack];
    uint64_t ownRooks   = b.rooks_bb[isBlack];
    uint64_t ownQueens  = b.queens_bb[isBlack];
    uint64_t ownKings   = b.kings_bb[isBlack];

    auto addMovesFromMask = [&](uint8_t from, uint64_t mask) {
        // Rimuovi le caselle occupate dai nostri pezzi
        uint64_t ownOcc = ownPawns | ownKnights | ownBishops | ownRooks | ownQueens | ownKings;
        mask &= ~ownOcc;

        while (mask) {
            const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(mask));
            mask &= (mask - 1);

            chess::Coords fromC{from};
            chess::Coords toC{to};

            if (!b.canMoveToBB(fromC, toC)) continue;

            moves.push_back(chess::Board::Move{fromC, toC});
        }
    };

    // Pawns
    while (ownPawns) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownPawns));
        ownPawns &= (ownPawns - 1);

        const uint64_t attacks = pieces::getPawnAttacks(static_cast<int16_t>(from), !isBlack);
        const uint64_t pushes  = pieces::getPawnForwardPushes(static_cast<int16_t>(from), !isBlack, occ);

        addMovesFromMask(from, attacks | pushes);
    }

    // Knights
    while (ownKnights) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownKnights));
        ownKnights &= (ownKnights - 1);

        const uint64_t mask = pieces::getKnightAttacks(static_cast<int16_t>(from));
        addMovesFromMask(from, mask);
    }

    // Bishops
    while (ownBishops) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownBishops));
        ownBishops &= (ownBishops - 1);

        const uint64_t mask = pieces::getBishopAttacks(static_cast<int16_t>(from), occ);
        addMovesFromMask(from, mask);
    }

    // Rooks
    while (ownRooks) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownRooks));
        ownRooks &= (ownRooks - 1);

        const uint64_t mask = pieces::getRookAttacks(static_cast<int16_t>(from), occ);
        addMovesFromMask(from, mask);
    }

    // Queens
    while (ownQueens) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownQueens));
        ownQueens &= (ownQueens - 1);

        const uint64_t mask = pieces::getQueenAttacks(static_cast<int16_t>(from), occ);
        addMovesFromMask(from, mask);
    }

    // Kings
    while (ownKings) {
        const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(ownKings));
        ownKings &= (ownKings - 1);

        const uint64_t mask = pieces::getKingAttacks(static_cast<int16_t>(from));
        addMovesFromMask(from, mask);
    }

#ifdef DEBUG
    // std::cout << "[DEBUG] generateLegalMoves_new found " << moves.size() << " moves.\n";
#endif

    return moves;
}

std::vector<Engine::ScoredMove> Engine::sortLegalMoves(const std::vector<chess::Board::Move>& moves, int ply, chess::Board& b, bool usIsWhite) {
    std::vector<ScoredMove> orderedScoredMoves;
    orderedScoredMoves.reserve(moves.size());
    for (const auto& m : moves) {
        int64_t score = 0;

        uint8_t fromPiece = b.get(m.from);
        uint8_t toPiece = b.get(m.to);
        uint8_t fromPieceType = fromPiece & chess::Board::MASK_PIECE_TYPE;
        uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;

        // MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
        if (toPieceType != chess::Board::EMPTY) {
            int64_t victimValue = pieceValues.at(toPieceType);
            int64_t attackerValue = pieceValues.at(fromPieceType);
            score += (victimValue * 10 - attackerValue); // MVV-LVA    
        }

        const bool isCapture = (toPieceType != chess::Board::EMPTY);

        if (fromPieceType == chess::Board::PAWN) {
            // Bonus per le promozioni
            if ((usIsWhite && m.to.rank == 7) || (!usIsWhite && m.to.rank == 0)) {
                score += pieceValues.at(chess::Board::QUEEN);
            }
        }

        // TODO: da rivedere, non sempre dare uno scacco e' la mossa migliore
        // in caso di scacco, aggiungi un bonus allo score
        chess::Board::MoveState tmpState;
        
        uint8_t piece = b.get(m.from);
        uint8_t pieceType  = piece & chess::Board::MASK_PIECE_TYPE;
        uint8_t pieceColor = piece & chess::Board::MASK_COLOR;

        const bool isPromotionCandidate =
            (pieceType == chess::Board::PAWN) &&
            ((pieceColor == chess::Board::WHITE && m.to.rank == 7) ||
                (pieceColor == chess::Board::BLACK && m.to.rank == 0));

        if (isPromotionCandidate) {
            b.doMove(m, tmpState, 'q');
        } else {
            b.doMove(m, tmpState);
        }

        uint8_t opponent = usIsWhite ? chess::Board::BLACK : chess::Board::WHITE;
        if (b.inCheck(opponent)) {
            score += CHECK_BONUS;
        }

        b.undoMove(m, tmpState);
        
        

        // Killer move bonus for non-captures
        if (!isCapture && ply < MAX_PLY) {
            const auto& km1 = killerMoves[0][ply];
            const auto& km2 = killerMoves[1][ply];
            if (m.from.file == km1.from.file && m.from.rank == km1.from.rank &&
                m.to.file == km1.to.file && m.to.rank == km1.to.rank) {
                score += KILLER1_BONUS;
            } else if (m.from.file == km2.from.file && m.from.rank == km2.from.rank &&
                       m.to.file == km2.to.file && m.to.rank == km2.to.rank) {
                score += KILLER2_BONUS;
            }

            // History heuristic bonus (only for non-captures)
            int colorIndex = (usIsWhite) ? 0 : 1;
            int fromIndex = m.from.rank * 8 + m.from.file;
            int toIndex   = m.to.rank   * 8 + m.to.file;
            score += history[colorIndex][fromIndex][toIndex];
        }

        // Penalizza fortemente le mosse di Re che non siano arrocco
        if (fromPieceType == chess::Board::KING) {
            // Heuristica semplice: considera arrocco se la mossa sposta il re di 2 colonne
            const int fileDelta = std::abs(m.to.file - m.from.file);
            const bool isCastlingLike = (fileDelta == 2);

            // TODO non penalizzare sempre: stare attenti a sacrifici o tattiche varie!!
            if (!isCastlingLike) { // Grossa penalità per spostare il re senza arroccare
                score -= KING_NON_CASTLING_PENALTY;
            }
            else { // TODO: NON SEMPRE ARROCCARE E' LA MOSSA MIGLIORE
                score += CASTLING_BONUS;
            }
        }

        orderedScoredMoves.push_back(ScoredMove{m, score});
    }

    std::sort(orderedScoredMoves.begin(), orderedScoredMoves.end(),
              [usIsWhite](const ScoredMove& a, const ScoredMove& b) {
                  return usIsWhite ? (a.score > b.score) : (a.score < b.score);
    });

    return orderedScoredMoves;
}
}; //namespace engine
