#pragma once

// Quantised network for NNUE v3: (768 -> 512)x2 -> 8, SCReLU, QA=255 QB=64,
// 8 material-count output buckets (bullet MaterialCount<8>):
//   bucket = (popcount(occupancy) - 2) / 4        // 2..32 pieces -> 0..7
//
// The struct mirrors bullet's quantised.bin byte-for-byte (little-endian i16;
// l1w saved TRANSPOSED so each bucket's 2*HIDDEN weights are contiguous);
// nnue/trainer/src/bin/sanity.rs is the reference reader and must stay
// consistent with this layout. Kept intentionally light: this header is
// included by board.hpp for the accumulator hot-path hooks.

#include <cstddef>
#include <cstdint>

namespace NNUE {

inline constexpr int INPUTS = 768;
inline constexpr int HIDDEN = 512;
inline constexpr int OUTPUT_BUCKETS = 8;
inline constexpr int32_t QA = 255;
inline constexpr int32_t QB = 64;
inline constexpr int32_t SCALE = 400;

struct alignas(64) Network {
    int16_t featureWeights[INPUTS][HIDDEN];          // l0w: column f = weights of feature f (QA)
    int16_t featureBias[HIDDEN];                     // l0b (QA)
    int16_t outputWeights[OUTPUT_BUCKETS][2][HIDDEN]; // l1w: [bucket][stm|ntm half] (QB)
    int16_t outputBias[OUTPUT_BUCKETS];              // l1b (QA*QB)
};

inline constexpr size_t NETWORK_PAYLOAD_BYTES =
    sizeof(int16_t) * (INPUTS * HIDDEN + HIDDEN
                       + OUTPUT_BUCKETS * 2 * HIDDEN + OUTPUT_BUCKETS);

static_assert(offsetof(Network, featureBias) == sizeof(int16_t) * INPUTS * HIDDEN);
static_assert(offsetof(Network, outputWeights) == offsetof(Network, featureBias) + sizeof(int16_t) * HIDDEN);
static_assert(offsetof(Network, outputBias) == offsetof(Network, outputWeights) + sizeof(int16_t) * OUTPUT_BUCKETS * 2 * HIDDEN);

// Non-null once a network is loaded (see NNUE::loadNetwork in nnue.hpp). Read
// in Board's piece-update hot path; written only with no search running.
extern const Network* activeNetwork;

} // namespace NNUE
