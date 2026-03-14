#include "movegen.hpp"
#include "../inl/bitboard_helpers.inl"
#include "../search/move_generator.hpp"

namespace engine {

MoveList<chess::Board::Move>
MoveGenerator::generateLegalMoves(const chess::Board& b) noexcept {
    return engine::search::MoveGenerator::generateLegalMoves(b);
}
} // namespace engine
