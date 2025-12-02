#include "engine.hpp"

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
        ttTable[i].key = 0;
        ttTable[i].depth = 0;
        ttTable[i].score = 0;
        ttTable[i].flag  = TTEntry::EXACT;
    }
}

bool Engine::shouldPruneLateMove(const chess::Board& b,const chess::Board::Move& m, int64_t depth, bool inCheck, bool usIsWhite, int moveIndex, int totalMoves){
    // Nessun late move pruning se poche mosse
    if (totalMoves <= 10) return false;

    // Applica solo a depth basse
    if (depth > 2) return false;

    // Non potare se siamo gia' in scacco
    if (inCheck) return false;

    // Calcola inizio dell'ultimo "decimo" di mosse (circa 10%)
    int lastTenth = totalMoves / 10;
    if (lastTenth < 1) lastTenth = 1;
    int latePruneStart = totalMoves - lastTenth;

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

void Engine::updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int64_t alpha, int64_t beta, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY]) {
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

} // namespace engine
