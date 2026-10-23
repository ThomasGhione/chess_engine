#pragma once

// The small network, for the positions that do not need the big one:
//
//   (768x4kb_hm -> 128)x2 -> pairwise -> 128 -> 8 -> 1
//
// The feature space of the big net (network.hpp: 4 mirrored king buckets, 8
// material output buckets), only narrower, so Board feeds both accumulators
// from the same move log. Same quantisation and arithmetic as
// network_deep.hpp. Trained by nnue/trainer/src/bin/trainer_small.rs; the
// reference reader is nnue/trainer/src/bin/sanity_deep.rs, which reads both
// layouts.
//
// FILE LAYOUT (little-endian, padded to 64 B with "bullet" repeated):
//
//   l0w [4*768][128]  i16  QA=255   (factoriser already folded in at save)
//   l0b [128]         i16  QA
//   l1w [8][8][128]   i8   QB=64    (transposed: each bucket contiguous)
//   l1b [8][8]        f32  real scale
//   l2w [8][1][8]     f32
//   l2b [8]           f32
//   payload 795,424 B, file 795,456 B
//
// ARITHMETIC (the big net's, line for line):
//
//   c[i] = clamp(acc[i], 0, QA)
//   p[j] = c[j] * c[j+64] / QA                   truncating integer division
//   z[o] = SUM(p[i] * l1w[o][i]) / (QA*QB) + l1b[o]   float division
//   a[o] = clamp(z[o], 0, 1)^2
//   y    = SUM a[o]*l2w[o] + l2b
//   cp   = round(y * SCALE)

#include <cstddef>
#include <cstdint>

#include "network.hpp"

namespace NNUE::Small {

inline constexpr int HIDDEN         = 128;
inline constexpr int L1_SIZE        = 8;   // = trainer_small.rs L1_SIZE
inline constexpr int OUTPUT_BUCKETS = 8;
inline constexpr int32_t QA    = 255;
inline constexpr int32_t QB    = 64;
inline constexpr int32_t SCALE = 400;   // the big net's: both evals share one scale

inline constexpr int PAIRWISE_OUT = HIDDEN / 2;
static_assert(HIDDEN % 64 == 0, "the SIMD pairwise works on 32-wide blocks per half");
static_assert(L1_SIZE % 4 == 0, "the SIMD dot product reduces four outputs at a time");

struct alignas(64) NetworkSmall {
    int16_t l0w[INPUT_BUCKETS * INPUTS][HIDDEN];
    int16_t l0b[HIDDEN];
    int8_t  l1w[OUTPUT_BUCKETS][L1_SIZE][HIDDEN];
    float   l1b[OUTPUT_BUCKETS][L1_SIZE];
    float   l2w[OUTPUT_BUCKETS][L1_SIZE];
    float   l2b[OUTPUT_BUCKETS];

    // l1w with each 32-wide block in the byte order the SIMD pairwise writes
    // (Deep::PACKUS_MAP), so the dot product needs no shuffle. Derived at load.
    alignas(64) int8_t l1wP[OUTPUT_BUCKETS][L1_SIZE][HIDDEN];
};

inline constexpr size_t PAYLOAD_BYTES =
      static_cast<size_t>(INPUT_BUCKETS) * INPUTS * HIDDEN * sizeof(int16_t)
    + static_cast<size_t>(HIDDEN) * sizeof(int16_t)
    + static_cast<size_t>(OUTPUT_BUCKETS) * L1_SIZE * HIDDEN * sizeof(int8_t)
    + static_cast<size_t>(OUTPUT_BUCKETS) * L1_SIZE * sizeof(float) * 2
    + static_cast<size_t>(OUTPUT_BUCKETS) * sizeof(float);
inline constexpr size_t FILE_BYTES = (PAYLOAD_BYTES + 63) / 64 * 64;
static_assert(PAYLOAD_BYTES == 795'424, "layout changed: update sanity_deep.rs");

// Scalar forward, the reference for the vectorised one.
[[nodiscard]] int32_t forwardScalar(const NetworkSmall& net, const int16_t* accStm,
                                    const int16_t* accNtm, int outputBucket) noexcept;

// The same function vectorised (AVX-VNNI, scalar without it): it MUST return
// forwardScalar's value for every input.
[[nodiscard]] int32_t forwardSimd(const NetworkSmall& net, const int16_t* accStm,
                                  const int16_t* accNtm, int outputBucket) noexcept;

// Reads a quantised.bin in the layout above and builds l1wP. False if the
// size or the padding is wrong.
[[nodiscard]] bool loadFromMemory(const unsigned char* data, size_t size, NetworkSmall& net) noexcept;

} // namespace NNUE::Small

namespace NNUE {

// Non-null once a small net is loaded (NNUE::Small::loadNetwork). Written
// only with no search running.
extern const Small::NetworkSmall* activeSmallNetwork;
// Counts the loads: the Finny tables tell nets apart by it, since a new net
// can be allocated where a freed one was.
extern uint32_t smallNetworkGeneration;

} // namespace NNUE
