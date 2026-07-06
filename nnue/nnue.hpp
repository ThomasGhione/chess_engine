#pragma once

// NNUE evaluation (NNUE_PLAN.md, Fase 3).
//
// `enabled` gates the branch in Evaluator::evaluate(); it can only be turned
// on once a network is loaded (UCI: setoption EvalFile, then UseNNUE). The
// accumulator maintenance on Board activates as soon as a network is loaded,
// independent of `enabled`, so toggling UseNNUE never leaves stale state.

#include <cstdint>
#include <string>

namespace chess { class Board; }

namespace NNUE {

// Flipped only by the UCI layer while no search is running (setOption stops
// any in-flight search first), so a plain bool is race-free.
inline bool enabled = false;

// Loads a bullet quantised.bin (validates size, padding signature and the
// output-weight bound the AVX2 forward relies on). Not for use mid-search.
[[nodiscard]] bool loadNetwork(const std::string& path);

[[nodiscard]] bool networkLoaded() noexcept;

// stm-relative centipawns from the board's incrementally-maintained
// accumulator. Requires networkLoaded().
[[nodiscard]] int32_t evaluate(const chess::Board& board) noexcept;

} // namespace NNUE
