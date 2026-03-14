#include "../engine.hpp"

#include "sorter.hpp"

namespace engine {

bool Engine::sortLegalMoves(
    MoveList<chess::Board::Move>& moves,
    int ply,
    chess::Board& b,
    bool usIsWhite,
    uint64_t hashKey,
    const chess::Board::Move* previousMove) noexcept {
    bool hashMoveIsLegal = false;
    moves = Sorter::sortLegalMoves(
        moves,
        ply,
        b,
        usIsWhite,
        hashKey,
        this->history,
        this->killerMoves,
        this->counterMoves,
        this->captureHistory,
        &this->tt,
        previousMove,
        &hashMoveIsLegal,
        ORDERING_PENALTY_SAME_PAWN_OPENING);
    return hashMoveIsLegal;
}

} // namespace engine
