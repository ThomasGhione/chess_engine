#include "engine.hpp"
#include "../board/piece.hpp"

namespace engine {

namespace {
inline int16_t clampHeuristic16(int32_t value) noexcept {
    constexpr int32_t MIN_I16 = -32768;
    constexpr int32_t MAX_I16 = 32767;
    return static_cast<int16_t>(std::clamp(value, MIN_I16, MAX_I16));
}
} // namespace

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
    ensureMagicTablesInitialized();
    this->tt.clear();
}

Engine::Engine(const std::string& fen)
    : Engine()
{
    board = chess::Board(fen);
}

Engine::~Engine() noexcept {
    this->stopPondering();
}

void Engine::reset() noexcept {
    this->stopPondering();
    board = chess::Board();
    depth = DEFAULTDEPTH;
    eval = 0;
    gameResult = ONGOING;
    isPlayerWhite = true;
    nodesSearched = 0;
    bestMove = chess::Board::Move{};
    moveHistory.clear();
    this->tt.clear();
    const chess::Board::Move emptyMove{};
    std::fill_n(&killerMoves[0][0], 2 * MAX_PLY, emptyMove);
    std::memset(history, 0, sizeof(history));
    std::memset(counterMoves, 0, sizeof(counterMoves));
    std::memset(captureHistory, 0, sizeof(captureHistory));
    this->ponderCurrentDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedDepth.store(0, std::memory_order_relaxed);
    this->ponderLastCompletedEvenDepth.store(0, std::memory_order_relaxed);
    this->ponderInterruptedDepth.store(0, std::memory_order_relaxed);
    this->ponderAspirationResearches.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailLow.store(0, std::memory_order_relaxed);
    this->ponderAspirationFailHigh.store(0, std::memory_order_relaxed);
}

void Engine::setPonderDebugEnabled(bool enabled) noexcept {
    this->ponderDebugEnabled.store(enabled, std::memory_order_relaxed);
}

bool Engine::isPonderDebugEnabled() const noexcept {
    return this->ponderDebugEnabled.load(std::memory_order_relaxed);
}

uint64_t Engine::getPonderCurrentDepth() const noexcept {
    return this->ponderCurrentDepth.load(std::memory_order_relaxed);
}

uint64_t Engine::getPonderLastCompletedDepth() const noexcept {
    return this->ponderLastCompletedDepth.load(std::memory_order_relaxed);
}

uint64_t Engine::getPonderInterruptedDepth() const noexcept {
    return this->ponderInterruptedDepth.load(std::memory_order_relaxed);
}


__attribute__((hot))
bool Engine::movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece) noexcept {
    this->stopPondering();

    const bool result = (promotionPiece == '\0')
        ? this->board.move(from, to)
        : this->board.move(from, to, promotionPiece);
    
    if (result) [[likely]] {
        appendMoveHistoryEntry(from, to, promotionPiece);
    }

    this->updateGameResult();

    return result;
}

void Engine::appendMoveHistoryEntry(const chess::Coords& from, const chess::Coords& to, char promotionPiece) noexcept {
    const size_t appendLen = (promotionPiece == '\0') ? size_t{5} : size_t{6};

    if (moveHistory.size() + appendLen > MOVE_HISTORY_MAX_BYTES) {
        const size_t overflow = (moveHistory.size() + appendLen) - MOVE_HISTORY_MAX_BYTES;
        const size_t firstErase = std::min(overflow, moveHistory.size());
        moveHistory.erase(0, firstErase);

        const size_t lineEnd = moveHistory.find('\n');
        if (lineEnd != std::string::npos) {
            moveHistory.erase(0, lineEnd + 1);
        } else {
            moveHistory.clear();
        }
    }

    moveHistory.reserve(moveHistory.size() + appendLen);
    moveHistory += from.toString();
    moveHistory += to.toString();
    if (promotionPiece != '\0') {
        moveHistory += promotionPiece;
    }
    moveHistory += '\n';
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


__attribute__((hot))
void Engine::updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int32_t depth, int ply, uint8_t us, int16_t (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY], const chess::Board::Move* previousMove) noexcept {
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
        const int32_t depthPlusOne = depth + 1;
        const int32_t bonus = static_cast<int32_t>(depthPlusOne * depthPlusOne);
        
        // GRAVITY FORMULA: prevents overflow and naturally saturates
        // h += bonus - h * |bonus| / MAX_CAPTURE_HISTORY
        constexpr int32_t MAX_CAPTURE_HISTORY = 10000;
        auto& chPrimary = captureHistory[colorIndex][toIndex][victimType][0];
        auto& chSecondary = captureHistory[colorIndex][toIndex][victimType][1];
        int32_t primaryScore = static_cast<int32_t>(chPrimary);
        primaryScore += bonus - primaryScore * std::abs(bonus) / MAX_CAPTURE_HISTORY;
        chPrimary = clampHeuristic16(primaryScore);

        const int32_t secondaryBonus = (bonus >> 1);
        int32_t secondaryScore = static_cast<int32_t>(chSecondary);
        secondaryScore += secondaryBonus - secondaryScore * std::abs(secondaryBonus) / MAX_CAPTURE_HISTORY;
        chSecondary = clampHeuristic16(secondaryScore);
        if (chSecondary > chPrimary) {
            std::swap(chPrimary, chSecondary);
        }
        
        return; // Don't process as quiet move
    }

    // COUNTER-MOVE: Track best response to opponent's previous move (quiet moves only)
    if (previousMove != nullptr && previousMove->from.index < 64) {
        counterMoves[previousMove->from.index][previousMove->to.index] =
            tt::TranspositionTable::Entry::encodeMove(fromIndex, toIndex, m.promotionPiece);
    }
    // KILLER MOVES: Update avoiding duplicates
    auto& km1 = killerMoves[0][ply];

    // Note: previously this used single '&'
    const bool isAlreadyKm1 = (fromIndex == km1.from.index) && (toIndex == km1.to.index);
    
    if (!isAlreadyKm1) {
	auto& km2 = killerMoves[1][ply];
        km2 = km1;
        km1 = m;
    }
    // If already km1, do nothing (avoid duplicates)

    // HISTORY HEURISTIC: Bonus based on depth
    // GRAVITY FORMULA: h += bonus - h * |bonus| / MAX_HISTORY
    const int colorIndex = (us == chess::Board::WHITE) ? 0 : 1;
    const int32_t depthPlusOne = depth + 1;
    const int32_t bonus = static_cast<int32_t>(depthPlusOne * depthPlusOne);
    
    constexpr int32_t MAX_HISTORY = 16384;
    auto& h = history[colorIndex][fromIndex][toIndex];
    int32_t historyScore = static_cast<int32_t>(h);
    historyScore += bonus - historyScore * std::abs(bonus) / MAX_HISTORY;
    h = clampHeuristic16(historyScore);
}

} // namespace engine
