#include "evaluator.hpp"

#include <limits>

#include "../nnue/nnue.hpp"

namespace engine {

int32_t Evaluator::evaluate(const chess::Board& board) noexcept {
    constexpr int32_t POS_INF = std::numeric_limits<int32_t>::max();
    const bool whiteToMove = (board.getActiveColor() == chess::Board::WHITE);
    if (board.kings_bb[0] == 0 || board.kings_bb[1] == 0) [[unlikely]] {
        if (board.kings_bb[0] == 0) return whiteToMove ? -POS_INF : POS_INF;
        return whiteToMove ? POS_INF : -POS_INF;
    }

    return NNUE::evaluate(board);
}

} // namespace engine
