#include "engine.hpp"
#include "../piece/piece.hpp"

namespace engine {
uint64_t Engine::nodesSearched = 0;
std::string Engine::moveHistory = "";

#ifdef DEBUG
uint64_t Engine::ttProbes = 0;
uint64_t Engine::ttHits = 0;
#endif

Engine::Engine()
    : board(chess::Board()),
    isPlayerWhite(true),
    depth(DEFAULTDEPTH),
    MAX_THREADS(omp_get_max_threads())
{
    // Inizializza magic bitboards una sola volta (thread-safe)
    static bool magicInitialized = false;
    if (!magicInitialized) {
        pieces::initMagicBitboards();
        magicInitialized = true;
    }

    // Inizializza TT (marca tutte le entries come INVALID)
    this->tt.clear();
    isCheckMate = false;
}

Engine::Engine(const std::string& fen)
    : board(chess::Board(fen)),
    isPlayerWhite(true),
    depth(DEFAULTDEPTH),
    MAX_THREADS(omp_get_max_threads())
{
    // Inizializza magic bitboards una sola volta (thread-safe)
    static bool magicInitialized = false;
    if (!magicInitialized) {
        pieces::initMagicBitboards();
        magicInitialized = true;
    }

    // Inizializza TT (marca tutte le entries come INVALID)
    this->tt.clear();
    isCheckMate = false;
}

void Engine::reset() noexcept {
    board = chess::Board();
    depth = DEFAULTDEPTH;
    eval = 0;
    isCheckMate = false;
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


__attribute__((hot))
bool Engine::shouldPruneLateMove(chess::Board& b,const chess::Board::Move& m, int32_t depth, bool inCheck, bool usIsWhite, int moveIndex, int totalMoves) noexcept {

    if (depth > 1) return false;
    if (inCheck) return false; // Don't prune if already in check
    if (totalMoves <= 10) return false; // Don't prune if too few moves

    
    const uint8_t toPiece = b.get(m.to); // Don't prune captures (cheap bitboard check)
    if ((toPiece & chess::Board::MASK_PIECE_TYPE) != chess::Board::EMPTY) return false;
    
    // Calculate late prune threshold (precalculate division)
    // latePruneStart = totalMoves - max(1, totalMoves / 10)
    const int lastTenth = (totalMoves >= 10) ? (totalMoves / 10) : 1;
    const int latePruneStart = totalMoves - lastTenth;

    if (moveIndex < latePruneStart) return false; // Not in late move zone yet

    // Check if move gives check (expensive check, do last)
    chess::Board::MoveState tmpState;
    b.doMove(m, tmpState);
    
    const uint8_t opponent = usIsWhite ? chess::Board::BLACK : chess::Board::WHITE;
    const bool givesCheck = b.inCheck(opponent);
    
    b.undoMove(m, tmpState);
    
    if (givesCheck) return false;

    // Quiet move in late position at low depth: prune
    return true;
}

// OPTIMIZED: Simplified killer logic, bitwise operations for bonus calculation
__attribute__((hot))
void Engine::updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int32_t depth, int ply, uint8_t us, int32_t alpha, int32_t beta, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY]) noexcept {
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
    const int32_t depthPlusOne = depth + 1;
    const int bonus = static_cast<int>(depthPlusOne * depthPlusOne);
    
    // OPTIMIZATION: Direct array access (already optimized)
    history[colorIndex][fromIndex][toIndex] += bonus;
    
    // OPTIONAL: Cap history values to prevent overflow (if needed)
    // constexpr int MAX_HISTORY = 10000;
    // if (history[colorIndex][fromIndex][toIndex] > MAX_HISTORY) {
    //     history[colorIndex][fromIndex][toIndex] = MAX_HISTORY;
    // }
}

} // namespace engine
