#pragma once

#include <cstdint>

#include "../board/board.hpp"

namespace engine {

// NNUE-only evaluation seam (the handcrafted evaluator was removed once the
// v1 net shipped in 2.0.0). Requires an active network:
// the embedded net is activated at startup in main(); datagen and selftest
// load theirs explicitly.
class Evaluator final {
public:
    Evaluator() = delete;

    // Side-to-move-relative score in centipawns.
    static int32_t evaluate(const chess::Board& board) noexcept;
};

} // namespace engine
