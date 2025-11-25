#include "engine.hpp"
#include "../coords/coords.hpp"

#include <omp.h>

#ifdef DEBUG
#include <chrono>
#include <iostream>
#endif

namespace engine {

uint64_t Engine::nodesSearched = 0;

#ifdef DEBUG
uint64_t Engine::ttProbes = 0;
uint64_t Engine::ttHits = 0;
uint64_t Engine::ttExactHits = 0;
uint64_t Engine::ttCutoffHits = 0;
#endif

Engine::Engine()
    : board(chess::Board())
    , depth(6)
{
    // this->nodesSearched = 0;
    // per ora non avviamo la search automaticamente nel costruttore
    // Collega la TT globale e inizializzala a valori noti
    ttTable = globalTT();
    for (std::size_t i = 0; i < TTEntry::TABLE_SIZE; ++i) {
        ttTable[i].key   = 0;
        ttTable[i].depth = 0;
        ttTable[i].score = 0;
        ttTable[i].flag  = TTEntry::EXACT;
    }
}

// -----------------------------------------------------------------------------
// Local helper constants for move ordering / pruning (can be tuned centrally)
// -----------------------------------------------------------------------------

namespace {

constexpr int64_t CHECK_BONUS                 = 50;       // bonus per dare scacco
constexpr int64_t KILLER1_BONUS               = 100000;   // bonus killer move primaria
constexpr int64_t KILLER2_BONUS               = 90000;    // bonus killer move secondaria
constexpr int64_t KING_NON_CASTLING_PENALTY   = 20000;    // penalita' per muovere il re senza arroccare
constexpr int64_t CASTLING_BONUS              = 5000;     // piccolo bonus per arroccare

} // namespace anonimo

int64_t Engine::getMaterialDeltaFAST(const chess::Board& b) noexcept {
    int64_t delta = 0;

    // White pieces: color index 0
    // Black pieces: color index 1

    // Pawns
    {
        int w = __builtin_popcountll(b.pawns_bb[0]);
        int bl = __builtin_popcountll(b.pawns_bb[1]);
        delta += static_cast<int64_t>(w - bl) * pieceValues.at(chess::Board::PAWN);
    }

    // Knights
    {
        int w = __builtin_popcountll(b.knights_bb[0]);
        int bl = __builtin_popcountll(b.knights_bb[1]);
        delta += static_cast<int64_t>(w - bl) * pieceValues.at(chess::Board::KNIGHT);
    }

    // Bishops
    {
        int w = __builtin_popcountll(b.bishops_bb[0]);
        int bl = __builtin_popcountll(b.bishops_bb[1]);
        delta += static_cast<int64_t>(w - bl) * pieceValues.at(chess::Board::BISHOP);
    }

    // Rooks
    {
        int w = __builtin_popcountll(b.rooks_bb[0]);
        int bl = __builtin_popcountll(b.rooks_bb[1]);
        delta += static_cast<int64_t>(w - bl) * pieceValues.at(chess::Board::ROOK);
    }

    // Queens
    {
        int w = __builtin_popcountll(b.queens_bb[0]);
        int bl = __builtin_popcountll(b.queens_bb[1]);
        delta += static_cast<int64_t>(w - bl) * pieceValues.at(chess::Board::QUEEN);
    }

    // Kings
    {
        int w = __builtin_popcountll(b.kings_bb[0]);
        int bl = __builtin_popcountll(b.kings_bb[1]);
        delta += static_cast<int64_t>(w - bl) * pieceValues.at(chess::Board::KING);
    }

    return delta;
}

int64_t Engine::getMaterialDelta(const chess::Board& b) noexcept {

	static constexpr auto coefficientPiece = [](uint8_t piece) {
	    return -2 *static_cast<int64_t>(piece >> 3) + 1;
  };

  static constexpr auto pieceValue = [](uint8_t x) {
    return static_cast<int64_t>( (x * (-134220 +
      x * (304540 +
      x * (-240405 +
      x * (87775 +
      x * (-15075 +
      x * 985)))))) / 36.0);
  };

  int64_t delta = 0;
  static constexpr uint8_t MAX_INDEX = 64;
  //#pragma omp parallel for reduction(+:delta)
  for (uint8_t i = 0; i < MAX_INDEX; i++) {
    uint8_t piece = b.get(i);

    delta += coefficientPiece(piece & chess::Board::MASK_COLOR) * pieceValue(piece & chess::Board::MASK_PIECE_TYPE);
  }
  
  return delta;
}

int64_t Engine::getMaterialDeltaSLOW(const chess::Board& b) noexcept {

    int64_t delta = 0;
    constexpr uint8_t MAX_INDEX = 64;

    for (uint8_t i = 0; i < MAX_INDEX; i++) {
        uint8_t piece = b.get(i);
        uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
        if (pieceType != chess::Board::EMPTY) {
            int64_t value = pieceValues.at(pieceType);
            int8_t colorFactor = piece & chess::Board::MASK_COLOR ? -1 : 1;
            delta += colorFactor * value;
        }
    }

    return delta;
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

void Engine::search(uint64_t depth) {
    if (depth == 0) return;

    std::vector<chess::Board::Move> moves = this->generateLegalMoves(this->board);

    if (moves.empty()) return;
    

    const uint8_t us = this->board.getActiveColor();
    const bool usIsWhite = (us == chess::Board::WHITE);

    this->nodesSearched = 0; // reset the nodes searched counter

#ifdef DEBUG
    ttProbes = ttHits = ttExactHits = ttCutoffHits = 0;
#endif

    // Alpha-beta is always from the side-to-move point of view:
    // White tries to maximise the score, Black to minimise it.
    int64_t alpha = NEG_INF;
    int64_t beta  = POS_INF;
    int64_t bestScore = (usIsWhite) ? NEG_INF : POS_INF;
    chess::Board::Move bestMove = moves.front(); // temporary initialization

    for (const auto& m : moves) {
        chess::Board copy = this->board;

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
            moveOk = copy.moveBB(m.from, m.to, 'q');
        } else {
            moveOk = copy.moveBB(m.from, m.to);
        }

        if (!moveOk) {
            continue;
        }
        constexpr int currPly = 1;
        int64_t score = this->searchPosition(copy, depth - 1, alpha, beta, currPly);

        this->updateMinMax(usIsWhite, score, alpha, beta, bestScore, bestMove, m);
    }

    // Esegui sulla board principale la mossa migliore trovata (gestendo promozioni)
    {
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

    // TODO spostare in driver
    std::string moveStr = chess::Coords::toAlgebric(bestMove.from) + chess::Coords::toAlgebric(bestMove.to);
    std::cout << "Engine plays: " << moveStr << " (score: " << bestScore << ")\n";

#ifdef DEBUG
    std::cout << "[DEBUG] TT probes: " << ttProbes
              << ", hits: " << ttHits
              << ", exact hits (approx): " << ttExactHits
              << ", cutoff hits: " << ttCutoffHits << "\n";
#endif
}

// -----------------------------------------------------------------------------
// Local helpers for searchPosition (TT handling, pruning, killer/history, etc.)
// -----------------------------------------------------------------------------

namespace {

inline bool shouldPruneLateMove(const chess::Board& b,
                                const chess::Board::Move& m,
                                int64_t depth,
                                bool inCheck,
                                bool usIsWhite,
                                int moveIndex,
                                int totalMoves) {
    // Nessun late move pruning se poche mosse
    if (totalMoves <= 10) return false;

    // Applica solo a depth basse
    if (depth > 2) return false;

    // Non potare se siamo gia' in scacco
    if (inCheck) return false;

    // Calcola inizio dell'ultimo "sesto" di mosse (circa ~16.7%)
    int lastSixth = totalMoves / 6;
    if (lastSixth < 1) lastSixth = 1;
    int latePruneStart = totalMoves - lastSixth;

    if (moveIndex < latePruneStart) return false; // non siamo in coda

    // Non potare catture e mosse che danno scacco
    uint8_t toPiece = b.get(m.to);
    uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
    const bool isCapture = (toPieceType != chess::Board::EMPTY);
    if (isCapture) return false;

    bool givesCheck = false;
    chess::Board checkBoard = b;
    if (checkBoard.moveBB(m.from, m.to)) {
        uint8_t opponent = usIsWhite ? chess::Board::BLACK : chess::Board::WHITE;
        givesCheck = checkBoard.inCheck(opponent);
    }

    if (givesCheck) return false;

    // Mosse silenziose in coda a depth bassa: prune
    return true;
}

inline void updateKillerAndHistoryOnBetaCutoff(const chess::Board& b,
                                               const chess::Board::Move& m,
                                               int64_t depth,
                                               int ply,
                                               uint8_t us,
                                               int64_t alpha,
                                               int64_t beta,
                                               int (&history)[2][64][64],
                                               chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY]) {
    if (alpha < beta) return; // nessun beta-cutoff

    if (ply >= Engine::MAX_PLY) return;

    uint8_t toPiece = b.get(m.to);
    uint8_t toPieceType = toPiece & chess::Board::MASK_PIECE_TYPE;
    const bool isCapture = (toPieceType != chess::Board::EMPTY);
    if (isCapture) return; // killer/history solo per non-catture

    // Killer moves
    auto& km1 = killerMoves[0][ply];
    auto& km2 = killerMoves[1][ply];
    if (!(m.from.file == km1.from.file && m.from.rank == km1.from.rank &&
          m.to.file   == km1.to.file   && m.to.rank   == km1.to.rank)) {
        km2 = km1;
        km1 = m;
    }

    // History heuristic
    int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
    int fromIndex = m.from.rank * 8 + m.from.file;
    int toIndex   = m.to.rank   * 8 + m.to.file;
    int bonus = static_cast<int>((depth + 1) * (depth + 1));
    history[colorIndex][fromIndex][toIndex] += bonus;
}

} // namespace anonimo

int64_t Engine::searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply) {
    this->nodesSearched++;  // una posizione visitata

    const uint8_t us = b.getActiveColor();
    const bool usIsWhite = (us == chess::Board::WHITE);

    if (depth == 0 || b.isCheckmate(us) || b.isStalemate(us)) {
        return this->evaluate(b);
    }

    bool inCheck = b.inCheck(us);
    if (inCheck && depth > 0) depth++; // extend search if in check

    // Prova a usare la transposition table solo per depth >= 2
    const uint64_t hashKey = computeHashKey(b);
    if (depth >= 2) {
#ifdef DEBUG
        ++ttProbes;
#endif
        const int32_t alpha32 = static_cast<int32_t>( 
            std::max<int64_t>(alpha, std::numeric_limits<int32_t>::min() + 1));
        const int32_t beta32  = static_cast<int32_t>(
            std::min<int64_t>(beta,  std::numeric_limits<int32_t>::max() - 1));
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
        chess::Board copy = b;
        if (!copy.moveBB(m.from, m.to)) continue;

        int64_t childDepth = depth - 1;
        if (childDepth < 0) childDepth = 0;

        // Late move pruning adattivo sulla coda delle mosse
        if (shouldPruneLateMove(b, m, depth, inCheck, usIsWhite, moveIndex, totalMoves)) {
            continue;
        }

        int64_t score = this->searchPosition(copy, childDepth, alpha, beta, ply + 1);

        this->updateMinMax(usIsWhite, score, alpha, beta, best);
        
        // TODO wrap in a function
        // Beta cutoff: update killer moves and history for non-captures
        if (alpha >= beta) {
            updateKillerAndHistoryOnBetaCutoff(b, m, depth, ply, us,
                                               alpha, beta,
                                               history, killerMoves);
            break; // alpha-beta cutoff
        }
        
    }

    // Determina il tipo di nodo per la TT e salva solo per depth >= 2
    if (depth >= 2) {
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

    return best;
}


// Nuova versione bitboard-based per tipo
// TODO fare un array di uint64_t ownPieces per fare un for sull'array per togliere codice duplicato + parallel for
std::vector<chess::Board::Move> Engine::generateLegalMoves(const chess::Board& b) const {
    std::vector<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const bool isWhite = (color == chess::Board::WHITE);
    // Usa occupancy e bitboard per tipo/colore dalla Board
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t ownPawns   = (color == chess::Board::WHITE) ? b.pawns_bb[0]   : b.pawns_bb[1];
    uint64_t ownKnights = (color == chess::Board::WHITE) ? b.knights_bb[0] : b.knights_bb[1];
    uint64_t ownBishops = (color == chess::Board::WHITE) ? b.bishops_bb[0] : b.bishops_bb[1];
    uint64_t ownRooks   = (color == chess::Board::WHITE) ? b.rooks_bb[0]   : b.rooks_bb[1];
    uint64_t ownQueens  = (color == chess::Board::WHITE) ? b.queens_bb[0]  : b.queens_bb[1];
    uint64_t ownKings   = (color == chess::Board::WHITE) ? b.kings_bb[0]   : b.kings_bb[1];

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

        const uint64_t attacks = pieces::getPawnAttacks(static_cast<int16_t>(from), isWhite);
        const uint64_t pushes  = pieces::getPawnForwardPushes(static_cast<int16_t>(from), isWhite, occ);

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

std::vector<Engine::ScoredMove> Engine::sortLegalMoves(const std::vector<chess::Board::Move>& moves, int ply, const chess::Board& b, bool usIsWhite) {
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
        chess::Board checkBoard = b;
        if (checkBoard.moveBB(m.from, m.to)) {
            uint8_t opponent = usIsWhite ? chess::Board::BLACK : chess::Board::WHITE;
            if (checkBoard.inCheck(opponent)) {
                score += CHECK_BONUS;
            }
        }

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


int64_t Engine::evaluateCheckmate(const chess::Board& board) {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

int64_t Engine::evaluate(const chess::Board& board) {

    // 1) EVALUATION CHECKMATE
    if (board.isCheckmate(board.getActiveColor())) {
        return this->evaluateCheckmate(board);
    }

    int64_t eval = 0;

    int64_t materialDelta = getMaterialDeltaFAST(board);
    eval += materialDelta;

    
    // 2) IS THIS AN ENDGAME?

    // count pieces (not including pawns, kings) using bitboards
    int nonPawnNonKingPieces =
        __builtin_popcountll(board.knights_bb[0] | board.knights_bb[1]) +
        __builtin_popcountll(board.bishops_bb[0] | board.bishops_bb[1]) +
        __builtin_popcountll(board.rooks_bb[0]   | board.rooks_bb[1]) +
        __builtin_popcountll(board.queens_bb[0]  | board.queens_bb[1]);

    bool isEndgame = (nonPawnNonKingPieces <= PHASE_FINAL_THRESHOLD);

    // 3) BONUS POSITION TABLE (bitboard-based per piece type)

    auto addPsqtFor = [&](uint64_t bbWhite, uint64_t bbBlack,
                           auto valueSelectorWhite,
                           auto valueSelectorBlack) {
        // White pieces
        uint64_t bb = bbWhite;
        while (bb) {
            uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bb));
            bb &= (bb - 1);

            uint8_t idx = sq;
            int64_t posValue = valueSelectorWhite(idx);
            eval += posValue; // white adds
        }

        // Black pieces
        bb = bbBlack;
        while (bb) {
            uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bb));
            bb &= (bb - 1);

            uint8_t idx = mirrorIndex(sq);
            int64_t posValue = valueSelectorBlack(idx);
            eval -= posValue; // black subtracts
        }
    };

    // Pawns
    addPsqtFor(
        board.pawns_bb[0],
        board.pawns_bb[1],
        [&](uint8_t idx) {
            return isEndgame ? PAWN_END_GAME_VALUES_TABLE[idx]
                             : PAWN_VALUES_TABLE[idx];
        },
        [&](uint8_t idx) {
            return isEndgame ? PAWN_END_GAME_VALUES_TABLE[idx]
                             : PAWN_VALUES_TABLE[idx];
        }
    );

    // Knights
    addPsqtFor(
        board.knights_bb[0],
        board.knights_bb[1],
        [&](uint8_t idx) { return KNIGHT_VALUES_TABLE[idx]; },
        [&](uint8_t idx) { return KNIGHT_VALUES_TABLE[idx]; }
    );

    // Bishops
    addPsqtFor(
        board.bishops_bb[0],
        board.bishops_bb[1],
        [&](uint8_t idx) { return BISHOP_VALUES_TABLE[idx]; },
        [&](uint8_t idx) { return BISHOP_VALUES_TABLE[idx]; }
    );

    // Rooks
    addPsqtFor(
        board.rooks_bb[0],
        board.rooks_bb[1],
        [&](uint8_t idx) { return ROOK_VALUES_TABLE[idx]; },
        [&](uint8_t idx) { return ROOK_VALUES_TABLE[idx]; }
    );

    // Queens
    addPsqtFor(
        board.queens_bb[0],
        board.queens_bb[1],
        [&](uint8_t idx) { return QUEEN_VALUES_TABLE[idx]; },
        [&](uint8_t idx) { return QUEEN_VALUES_TABLE[idx]; }
    );

    // Kings
    addPsqtFor(
        board.kings_bb[0],
        board.kings_bb[1],
        [&](uint8_t idx) {
            return isEndgame ? KING_END_GAME_VALUES_TABLE[idx]
                             : KING_MIDDLE_GAME_VALUES_TABLE[idx];
        },
        [&](uint8_t idx) {
            return isEndgame ? KING_END_GAME_VALUES_TABLE[idx]
                             : KING_MIDDLE_GAME_VALUES_TABLE[idx];
        }
    );



    int64_t whiteEval = 0, blackEval = 0;
    
    eval += (whiteEval - blackEval);
    return eval;
}

bool Engine::isMate() {
    uint8_t toMove = this->board.getActiveColor();
    if (this->board.isCheckmate(toMove) || this->board.isStalemate(toMove)) {
        return true;
    }
    return false;
}

} // namespace engine
