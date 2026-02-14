#include "evaluator.hpp"

namespace engine {

int64_t Evaluator::evalMinorPieceDevelopment(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_MINOR_START = 0xFF00000000000000ULL; // rank 1 & 2 (bit 56-63 + 48-55)
    static constexpr uint64_t BLACK_MINOR_START = 0x000000000000FFFFULL; // rank 8 & 7 (bit 0-7 + 8-15)
    
    // Conta pezzi sviluppati (fuori dalle caselle iniziali) in una sola operazione
    const int whiteDeveloped = 
        __builtin_popcountll(b.knights_bb[0] & ~WHITE_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[0] & ~WHITE_MINOR_START);
    
    const int blackDeveloped = 
        __builtin_popcountll(b.knights_bb[1] & ~BLACK_MINOR_START) +
        __builtin_popcountll(b.bishops_bb[1] & ~BLACK_MINOR_START);
    
    return (whiteDeveloped - blackDeveloped) * DEVELOPMENT_BONUS;
}

int64_t Evaluator::evalEarlyQueen(const chess::Board& b) noexcept {
    static constexpr uint64_t WHITE_QUEEN_START = chess::Board::bitMask(59); // d1
    static constexpr uint64_t BLACK_QUEEN_START = chess::Board::bitMask(3);  // d8
    static constexpr int64_t EARLY_QUEEN_DEV_PENALTY = 20;
    
    int64_t score = 0;

    score -= (b.queens_bb[0] && !(b.queens_bb[0] & WHITE_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;
    score += (b.queens_bb[1] && !(b.queens_bb[1] & BLACK_QUEEN_START)) * EARLY_QUEEN_DEV_PENALTY;

    return score;
}

} // namespace engine
