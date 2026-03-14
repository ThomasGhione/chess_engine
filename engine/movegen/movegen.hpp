#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include <cstdint>
#include "../../board/board.hpp"
#include "../movelist.hpp"

namespace engine {

class MoveGenerator final {
    
public:
    MoveGenerator() = delete;  // Static class, no instantiation
    
    static MoveList<chess::Board::Move> generateLegalMoves(const chess::Board& b) noexcept;
    
    static MoveList<chess::Board::Move> generateTacticalMoves(
        const chess::Board& b,
        bool includeChecks = false,
        bool inCheckKnown = false,
        bool inCheckValue = false,
        bool inDoubleCheckValue = false) noexcept;
};

} // namespace engine

#endif // MOVEGEN_HPP
