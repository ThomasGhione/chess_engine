#pragma once

// NNUE evaluation. A network must be active before any evaluate(): main()
// activates the embedded one, EvalFile replaces it, datagen and the selftest
// load their own.

#include <cstdint>
#include <string>

#include "network_small.hpp"

namespace chess { class Board; }

namespace NNUE {

// Loads a bullet quantised.bin (validates size and padding signature). Not for
// use mid-search.
[[nodiscard]] bool loadNetwork(const std::string& path);

// Makes the network compiled into the binary active (nnue/net/hydray.nnue via
// nnue/embedded.cpp); false if the blob fails validation.
[[nodiscard]] bool activateEmbedded() noexcept;

// stm-relative centipawns from the board's incrementally-maintained
// accumulator. Requires an active network.
[[nodiscard]] int32_t evaluate(const chess::Board& board) noexcept;

} // namespace NNUE

// The small net (network_small.hpp), next to the big one: which of the two
// evaluates a position is the caller's choice. Same centipawn scale. The eval
// cache and the TT's staticEval are keyed by position only, so a choice that
// depends on anything but the position mixes the two nets' evals there.
namespace NNUE::Small {

// Loads a small-net quantised.bin and makes it active. Not for use mid-search.
[[nodiscard]] bool loadNetwork(const std::string& path);

[[nodiscard]] inline bool loaded() noexcept { return activeSmallNetwork != nullptr; }

// stm-relative centipawns from the board's small accumulator, computed on
// demand from the same move log as the big one. Requires loaded().
[[nodiscard]] int32_t evaluate(const chess::Board& board) noexcept;

} // namespace NNUE::Small
