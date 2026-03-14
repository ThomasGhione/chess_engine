#include "movegen.hpp"

#include "../search/move_generator.hpp"

namespace engine {

MoveList<chess::Board::Move> MoveGenerator::generateTacticalMoves(
    const chess::Board& b,
    bool includeChecks,
    bool inCheckKnown,
    bool inCheckValue,
    bool inDoubleCheckValue) noexcept {
    return engine::search::MoveGenerator::generateTacticalMoves(
        b,
        includeChecks,
        inCheckKnown,
        inCheckValue,
        inDoubleCheckValue);
}

} // namespace engine
