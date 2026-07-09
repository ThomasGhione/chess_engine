#pragma once

// Quantised network for NNUE v2: (768 -> 512)x2 -> 1, SCReLU, QA=255 QB=64.
//
// The struct mirrors bullet's quantised.bin byte-for-byte (little-endian i16,
// column-major); nnue/trainer/src/bin/sanity.rs is the reference reader and
// must stay consistent with this layout. Kept intentionally light: this header
// is included by board.hpp for the accumulator hot-path hooks.

#include <cstddef>
#include <cstdint>

namespace NNUE {

inline constexpr int INPUTS = 768;
inline constexpr int HIDDEN = 512;
inline constexpr int32_t QA = 255;
inline constexpr int32_t QB = 64;
inline constexpr int32_t SCALE = 400;

struct alignas(64) Network {
    int16_t featureWeights[INPUTS][HIDDEN]; // l0w: column f = weights of feature f (QA)
    int16_t featureBias[HIDDEN];            // l0b (QA)
    int16_t outputWeights[2][HIDDEN];       // l1w: [0] = stm half, [1] = ntm half (QB)
    int16_t outputBias;                     // l1b (QA*QB)
};

inline constexpr size_t NETWORK_PAYLOAD_BYTES =
    sizeof(int16_t) * (INPUTS * HIDDEN + HIDDEN + 2 * HIDDEN + 1);

static_assert(offsetof(Network, featureBias) == sizeof(int16_t) * INPUTS * HIDDEN);
static_assert(offsetof(Network, outputWeights) == offsetof(Network, featureBias) + sizeof(int16_t) * HIDDEN);
static_assert(offsetof(Network, outputBias) == offsetof(Network, outputWeights) + sizeof(int16_t) * 2 * HIDDEN);

// Non-null once a network is loaded (see NNUE::loadNetwork in nnue.hpp). Read
// in Board's piece-update hot path; written only with no search running.
extern const Network* activeNetwork;

} // namespace NNUE
