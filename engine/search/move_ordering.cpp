#include "../engine.hpp"

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

} // namespace engine
