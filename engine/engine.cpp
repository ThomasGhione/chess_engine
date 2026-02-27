#include "engine.hpp"
#include "../board/piece.hpp"

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
    ensureMagicTablesInitialized();
    this->tt.clear();
}

Engine::Engine(const std::string& fen)
    : board(chess::Board(fen))
    , isPlayerWhite(true)
    , depth(DEFAULTDEPTH)
    , MAX_THREADS(omp_get_max_threads())
{
    ensureMagicTablesInitialized();
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
    moveHistory.clear();
#ifdef DEBUG
    ttProbes = 0;
    ttHits = 0;
#endif
    this->tt.clear();
    const chess::Board::Move emptyMove{};
    std::fill_n(&killerMoves[0][0], 2 * MAX_PLY, emptyMove);
    std::memset(history, 0, sizeof(history));
    std::fill_n(&counterMoves[0][0], 64 * 64, emptyMove);
    std::memset(captureHistory, 0, sizeof(captureHistory));
}


__attribute__((hot))
bool Engine::movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece) noexcept {
    bool result;
    if (promotionPiece == '\0') {
        result = this->board.move(from, to);
    } else {
        result = this->board.move(from, to, promotionPiece);
    }
    
    if (result) [[likely]] {
        moveHistory.reserve(moveHistory.size() + 6); // "e2e4\n" = 5 chars max
        moveHistory += from.toString();
        moveHistory += to.toString();
        if (promotionPiece == '\0') {
            moveHistory += '\n';
        } else {
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


__attribute__((hot))
void Engine::updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int32_t (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY], const chess::Board::Move* previousMove) noexcept {
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
        const int32_t bonus = static_cast<int32_t>(depthPlusOne * depthPlusOne);
        
        // GRAVITY FORMULA: prevents overflow and naturally saturates
        // h += bonus - h * |bonus| / MAX_CAPTURE_HISTORY
        constexpr int32_t MAX_CAPTURE_HISTORY = 10000;
        auto& chPrimary = captureHistory[colorIndex][toIndex][victimType][0];
        auto& chSecondary = captureHistory[colorIndex][toIndex][victimType][1];
        chPrimary += bonus - chPrimary * std::abs(bonus) / MAX_CAPTURE_HISTORY;

        const int32_t secondaryBonus = (bonus >> 1);
        chSecondary += secondaryBonus - chSecondary * std::abs(secondaryBonus) / MAX_CAPTURE_HISTORY;
        if (chSecondary > chPrimary) {
            std::swap(chPrimary, chSecondary);
        }
        
        return; // Don't process as quiet move
    }

    // COUNTER-MOVE: Track best response to opponent's previous move (quiet moves only)
    if (previousMove != nullptr && previousMove->from.index < 64) {
        counterMoves[previousMove->from.index][previousMove->to.index] = m;
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
    const int64_t depthPlusOne = depth + 1;
    const int32_t bonus = static_cast<int32_t>(depthPlusOne * depthPlusOne);
    
    constexpr int32_t MAX_HISTORY = 16384;
    auto& h = history[colorIndex][fromIndex][toIndex];
    h += bonus - h * std::abs(bonus) / MAX_HISTORY;
}

} // namespace engine
