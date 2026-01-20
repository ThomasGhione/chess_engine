#include "engine.hpp"
#include "../piece/piece.hpp"

namespace engine {

#ifdef DEBUG
uint64_t Engine::ttProbes = 0;
uint64_t Engine::ttHits = 0;
#endif

Engine::Engine()
    : board(chess::Board())
    , isPlayerWhite(true)
    , depth(DEFAULTDEPTH)
    , MAX_THREADS(omp_get_max_threads())
{
    // Inizializza magic bitboards una sola volta (thread-safe)
    static bool magicInitialized = false;
    if (!magicInitialized) {
        pieces::initMagicBitboards();
        magicInitialized = true;
    }

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
    static bool magicInitialized = false;
    if (!magicInitialized) {
        pieces::initMagicBitboards();
        magicInitialized = true;
    }

    // Inizializza TT (marca tutte le entries come INVALID)
    this->tt.clear();
}

void Engine::reset() noexcept {
    board = chess::Board();
    depth = DEFAULTDEPTH;
    eval = 0;
    gameResult = ONGOING;
    isPlayerWhite = true;
    nodesSearched = 0;
    
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

    return result;
}


// OPTIMIZED: Simplified killer logic, bitwise operations for bonus calculation
__attribute__((hot))
void Engine::updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int64_t alpha, int64_t beta, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY]) noexcept {
    // EARLY EXIT: cheap checks first
    if (alpha < beta) return; // No beta-cutoff
    if (ply >= Engine::MAX_PLY) return; // Out of bounds
    
    // Only update for non-captures (killer/history heuristic)
    const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
    if (toPieceType != chess::Board::EMPTY) return;
    
    const uint8_t fromIndex = m.from.index;
    const uint8_t toIndex = m.to.index;

    // KILLER MOVES: Update if not already primary killer
    auto& km1 = killerMoves[0][ply];
    auto& km2 = killerMoves[1][ply];
    const bool isAlreadyKm1 = (fromIndex == km1.from.index) & (toIndex == km1.to.index);
    if (!isAlreadyKm1) {
        km2 = km1;
        km1 = m;
    }

    // HISTORY HEURISTIC: Bonus based on depth
    // bonus = (depth + 1) * (depth + 1) can be optimized for small depths
    const int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
    const int64_t depthPlusOne = depth + 1;
    const int bonus = static_cast<int>(depthPlusOne * depthPlusOne);
    
    // OPTIMIZATION: Direct array access (already optimized)
    history[colorIndex][fromIndex][toIndex] += bonus;
    
    // Cap history values to prevent overflow and stale data dominance
    // After ~30-40 beta cutoffs at depth 10, values would saturate at cap
    constexpr int MAX_HISTORY = 10000;
    if (history[colorIndex][fromIndex][toIndex] > MAX_HISTORY) {
        history[colorIndex][fromIndex][toIndex] = MAX_HISTORY;
    }
}

} // namespace engine
