#pragma once

// NNUE evaluation seam (NNUE_PLAN.md, Fase 0).
//
// No network exists yet: networkLoaded() is false, so the UCI layer refuses to
// turn `enabled` on and the [[unlikely]] branch in Evaluator::evaluate() stays
// dead. Fase 3 adds the accumulator, the AVX2 forward pass and file/incbin
// loading behind this same interface.

#include <cstdint>

namespace chess { class Board; }

namespace NNUE {

// Flipped only by the UCI layer while no search is running (setOption stops
// any in-flight search first), so a plain bool is race-free.
inline bool enabled = false;

[[nodiscard]] bool networkLoaded() noexcept;
[[nodiscard]] int32_t evaluate(const chess::Board& board) noexcept;

} // namespace NNUE
