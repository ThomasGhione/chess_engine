#include "engine.hpp"
#include "../piece/piece.hpp"

namespace engine {

#ifdef DEBUG
uint64_t Engine::ttProbes = 0;
uint64_t Engine::ttHits = 0;
#endif

// Static method to ensure magic tables are initialized exactly once
void Engine::ensureMagicTablesInitialized() noexcept {
    if (!magicTablesInitialized) {
        pieces::initMagicBitboards();
        magicTablesInitialized = true;
    }
}

Engine::Engine()
    : board(chess::Board())
    , isPlayerWhite(true)
    , depth(DEFAULTDEPTH)
    , MAX_THREADS(omp_get_max_threads())
{
    // Inizializza magic bitboards una sola volta (thread-safe)
    ensureMagicTablesInitialized();

    // Inizializza TT (marca tutte le entries come INVALID)
    this->tt.clear();
}

Engine::Engine(const std::string& fen)
    : board(chess::Board(fen))
    , isPlayerWhite(true)
    , depth(DEFAULTDEPTH)
    , MAX_THREADS(omp_get_max_threads())
{
    // Inizializza magic bitboards una sola volta (thread-safe)
    ensureMagicTablesInitialized();

    // Inizializza TT (marca tutte le entries come INVALID)
    this->tt.clear();
}

void Engine::reset() noexcept {
    board = chess::Board();
    depth = DEFAULTDEPTH;
    UCI_DEPTH = 0;
    eval = 0;
    gameResult = ONGOING;
    isPlayerWhite = true;
    nodesSearched = 0;
    bestMove = chess::Board::Move{};
    
    // Reset endgame depth extension flags
    depthExtended = false; 
    
    // Reset move history
    moveHistory.clear();
    
#ifdef DEBUG
    ttProbes = 0;
    ttHits = 0;
#endif

    // Reset TT
    this->tt.clear();

    // Reset killer moves
    for (int ply = 0; ply < MAX_PLY; ++ply) {
        killerMoves[0][ply] = chess::Board::Move{};
        killerMoves[1][ply] = chess::Board::Move{};
    }

    // Reset history heuristic
    std::memset(history, 0, sizeof(history));

    // Reset counter-move history
    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            counterMoves[from][to] = chess::Board::Move{};
        }
    }

    // Reset capture history
    std::memset(captureHistory, 0, sizeof(captureHistory));
}


__attribute__((hot))
bool Engine::movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece) noexcept {
    bool result;
    if (promotionPiece == '\0') [[likely]] {
        result = this->board.moveBB(from, to);
    } else [[unlikely]] {
        result = this->board.moveBB(from, to, promotionPiece);
    }
    
    if (result) [[likely]] {
        // OPTIMIZATION: Reserve space and use single append instead of multiple concatenations
        moveHistory.reserve(moveHistory.size() + 6); // "e2e4\n" = 5 chars max
        moveHistory += from.toString();
        moveHistory += to.toString();
        if (promotionPiece == '\0') [[likely]] {
            moveHistory += '\n';
        } else [[unlikely]] {
            moveHistory += promotionPiece;
            moveHistory += '\n';
        }
    }

    this->updateGameResult();

    return result;
}


void Engine::updateGameResult() noexcept {
    gameResult = GameResult::ONGOING;
    uint8_t toMove = board.getActiveColor();
    if (board.kings_bb[0] == 0) {
        gameResult = GameResult::BLACK_WINS;
    } else if (board.kings_bb[1] == 0) {
        gameResult = GameResult::WHITE_WINS;
    } else if (board.isCheckmate(toMove)) {
        gameResult = (toMove == chess::Board::WHITE) ? GameResult::BLACK_WINS : GameResult::WHITE_WINS;
    } else if (board.isDraw(toMove)) {
        gameResult = GameResult::DRAW;
    }
}


// OPTIMIZED: Simplified killer logic, bitwise operations for bonus calculation
__attribute__((hot))
void Engine::updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY], const chess::Board::Move* previousMove) noexcept {
    if (ply >= Engine::MAX_PLY) return; // Out of bounds
    
    const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
    const bool isEpCapture = isEnPassantCapture(b, m);
    const bool isCapture = (toPieceType != chess::Board::EMPTY) || isEpCapture;
    const uint8_t victimType = isEpCapture ? static_cast<uint8_t>(chess::Board::PAWN) : toPieceType;
    
    const uint8_t fromIndex = m.from.index;
    const uint8_t toIndex = m.to.index;

    // CAPTURE HISTORY: bonus for captures that cause cutoffs
    if (isCapture) {
        const int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
        const int64_t depthPlusOne = depth + 1;
        const int bonus = static_cast<int>(depthPlusOne * depthPlusOne);
        
        // GRAVITY FORMULA: prevents overflow and naturally saturates
        // h += bonus - h * |bonus| / MAX_CAPTURE_HISTORY
        constexpr int MAX_CAPTURE_HISTORY = 10000;
        auto& ch = captureHistory[colorIndex][toIndex][victimType];
        ch += bonus - ch * std::abs(bonus) / MAX_CAPTURE_HISTORY;
        
        return; // Don't process as quiet move
    }

    // COUNTER-MOVE: Track best response to opponent's previous move (quiet moves only)
    if (previousMove != nullptr && previousMove->from.index < 64) {
        counterMoves[previousMove->from.index][previousMove->to.index] = m;
    }
    // KILLER MOVES: Update avoiding duplicates
    auto& km1 = killerMoves[0][ply];
    auto& km2 = killerMoves[1][ply];
    const bool isAlreadyKm1 = (fromIndex == km1.from.index) & (toIndex == km1.to.index);
    const bool isAlreadyKm2 = (fromIndex == km2.from.index) & (toIndex == km2.to.index);
    
    if (!isAlreadyKm1) {
        // If it's km2, promote it to km1
        if (isAlreadyKm2) {
            km2 = km1;
            km1 = m;
        } else {
            // New killer: shift and insert
            km2 = km1;
            km1 = m;
        }
    }
    // If already km1, do nothing (avoid duplicates)

    // HISTORY HEURISTIC: Bonus based on depth
    // GRAVITY FORMULA: h += bonus - h * |bonus| / MAX_HISTORY
    // Prevents overflow naturally and allows negative values
    const int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
    const int64_t depthPlusOne = depth + 1;
    const int bonus = static_cast<int>(depthPlusOne * depthPlusOne);
    
    constexpr int MAX_HISTORY = 16384;
    auto& h = history[colorIndex][fromIndex][toIndex];
    h += bonus - h * std::abs(bonus) / MAX_HISTORY;
}

} // namespace engine
