#include "../engine.hpp"
#include "../tt.hpp"

namespace engine {

// Helper to add MVV (Most Valuable Victim) bonus for captures
// Simplified from MVV-LVA: only victim matters, attacker is irrelevant (SEE handles exchange eval)
void Engine::addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) noexcept {
    const uint8_t fromPieceType = b.get(m.from) & chess::Board::MASK_PIECE_TYPE;
    const uint8_t toPieceType   = b.get(m.to)   & chess::Board::MASK_PIECE_TYPE;

    if (toPieceType != chess::Board::EMPTY) {
        score += MVV_TABLE[toPieceType];  // MVV-only: just victim value
        return;
    }

    // En passant (only pawn moving diagonally to empty square)
    if (fromPieceType == chess::Board::PAWN) {
        if (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index)) {
            score += MVV_TABLE[chess::Board::PAWN];
        }
    }
}


// Helper to add promotion bonus
void Engine::addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score) noexcept {
    if (pieceType == chess::Board::PAWN) {
        if (m.to.rank() == chess::Board::promotionRank(usIsWhite)) {
            const char promo = static_cast<char>(std::tolower(static_cast<unsigned char>(m.promotionPiece)));
            uint8_t promoType = chess::Board::QUEEN; // default if promo char is missing
            if (promo == 'r') promoType = chess::Board::ROOK;
            else if (promo == 'b') promoType = chess::Board::BISHOP;
            else if (promo == 'n') promoType = chess::Board::KNIGHT;
            score += PIECE_VALUES[promoType];
        }
    }
}

// Helper to add check bonus
void Engine::addCheckBonus(const chess::Board::Move& m, chess::Board& b, bool usIsWhite, int64_t& score) noexcept {
    chess::Board::MoveState tmpState;
    doMoveWithPromotion(b, m, tmpState);
    if (b.inCheck(!usIsWhite)) {
        score += CHECK_BONUS;
    }
    b.undoMove(m, tmpState);
}

// Helper to add killer move and history heuristic bonuses
void Engine::addKillerAndHistoryBonus(const chess::Board::Move& m, int ply, bool usIsWhite, int64_t& score) noexcept {
    if (ply >= MAX_PLY) return;

    const auto& km1 = killerMoves[0][ply];
    const auto& km2 = killerMoves[1][ply];

    if (m.from.index == km1.from.index && m.to.index == km1.to.index) {
        score += KILLER1_BONUS;
    } else if (m.from.index == km2.from.index && m.to.index == km2.to.index) {
        score += KILLER2_BONUS;
    }

    const int colorIndex = chess::Board::colorBoolToIndex(usIsWhite);
    const int fromIndex = m.from.index;
    const int toIndex = m.to.index;
    score += history[colorIndex][fromIndex][toIndex];
}

// Helper to add king move heuristic bonus/penalty
// NOTE: inCheck precomputed outside the loop to avoid repeated calls
void Engine::addKingMoveBonus(const chess::Board::Move& m, uint8_t pieceType, bool inCheck, int fullMoveClock, int64_t& score) noexcept {
    if (pieceType != chess::Board::KING) return;

    const int fileDelta = std::abs(chess::Board::fileOf(m.to.index) - chess::Board::fileOf(m.from.index));
    const bool isCastling = (fileDelta == 2);

    if (fullMoveClock < 10 && !inCheck && !isCastling) {
        score -= KING_NON_CASTLING_PENALTY;
    } else if (isCastling) { 
        score += CASTLING_BONUS;
    }
}

} // namespace engine
