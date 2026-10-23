#include "network_small.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include "../board/board.hpp"
#include "network_deep.hpp"
#include "nnue.hpp"

#if defined(__AVX2__) && defined(__AVXVNNI__)
#include <immintrin.h>
#endif

namespace NNUE {

const Small::NetworkSmall* activeSmallNetwork = nullptr;
uint32_t smallNetworkGeneration = 0;

namespace Small {

namespace {

std::unique_ptr<NetworkSmall> owned;

// l1 sums -> centipawns. One function for both forwards, so the float tail is
// the same operations in the same order and the two stay bit-identical.
[[nodiscard]] inline int32_t tail(const NetworkSmall& net, const int32_t* sums, int bucket) noexcept {
    float y = net.l2b[bucket];
    for (int o = 0; o < L1_SIZE; ++o) {
        const float z = static_cast<float>(sums[o]) / static_cast<float>(QA * QB) + net.l1b[bucket][o];
        const float c = std::clamp(z, 0.0f, 1.0f);
        y += c * c * net.l2w[bucket][o];
    }
    return static_cast<int32_t>(std::lround(y * static_cast<float>(SCALE)));
}

inline void pairwise(const int16_t* acc, int32_t* out) noexcept {
    for (int j = 0; j < PAIRWISE_OUT; ++j) {
        const int32_t a = std::clamp<int32_t>(acc[j], 0, QA);
        const int32_t b = std::clamp<int32_t>(acc[j + PAIRWISE_OUT], 0, QA);
        out[j] = a * b / QA;
    }
}

} // namespace

int32_t forwardScalar(const NetworkSmall& net, const int16_t* accStm,
                      const int16_t* accNtm, int outputBucket) noexcept {
    int32_t hl1[HIDDEN];
    pairwise(accStm, hl1);
    pairwise(accNtm, hl1 + PAIRWISE_OUT);
    int32_t sums[L1_SIZE];
    for (int o = 0; o < L1_SIZE; ++o) {
        const int8_t* row = net.l1w[outputBucket][o];
        int32_t sum = 0;
        for (int i = 0; i < HIDDEN; ++i) sum += hl1[i] * static_cast<int32_t>(row[i]);
        sums[o] = sum;
    }
    return tail(net, sums, outputBucket);
}

int32_t forwardSimd(const NetworkSmall& net, const int16_t* accStm,
                    const int16_t* accNtm, int outputBucket) noexcept {
#if !(defined(__AVX2__) && defined(__AVXVNNI__))
    return forwardScalar(net, accStm, accNtm, outputBucket);
#else
    // The big net's pairwise block (nnue_deep.cpp says why each step is exact):
    // 32 outputs per block, left in Deep::PACKUS_MAP order, which l1wP absorbs.
    const auto block = [](const int16_t* acc, int j) noexcept {
        const auto ld = [](const int16_t* p) noexcept {
            return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
        };
        const __m256i ones = _mm256_set1_epi8(1);
        const __m256i a = _mm256_packus_epi16(ld(acc + j), ld(acc + j + 16));
        const __m256i b = _mm256_packus_epi16(ld(acc + j + PAIRWISE_OUT), ld(acc + j + PAIRWISE_OUT + 16));
        const auto half = [&](bool low) noexcept {
            const __m256i xa = low ? _mm256_unpacklo_epi8(a, a) : _mm256_unpackhi_epi8(a, a);
            const __m256i yb = low ? _mm256_unpacklo_epi8(ones, b) : _mm256_unpackhi_epi8(ones, b);
            return _mm256_srli_epi16(_mm256_mulhi_epu16(xa, yb), 8);
        };
        return _mm256_packus_epi16(half(true), half(false));
    };
    constexpr int BLOCKS = HIDDEN / 32;
    __m256i h[BLOCKS];
    for (int j = 0; j < PAIRWISE_OUT; j += 32) {
        h[j / 32] = block(accStm, j);
        h[(PAIRWISE_OUT + j) / 32] = block(accNtm, j);
    }

    // Dense dot product, one accumulator per output: at this width the sparse
    // input-driven loop of the big net would cost more than it skips.
    alignas(16) int32_t sums[L1_SIZE];
    for (int o = 0; o < L1_SIZE; o += 4) {
        __m256i acc[4];
        for (int k = 0; k < 4; ++k) {
            acc[k] = _mm256_setzero_si256();
            const int8_t* w = net.l1wP[outputBucket][o + k];
            for (int b = 0; b < BLOCKS; ++b) {
                acc[k] = _mm256_dpbusd_avx_epi32(acc[k], h[b],
                    _mm256_load_si256(reinterpret_cast<const __m256i*>(w + 32 * b)));
            }
        }
        const __m256i s = _mm256_hadd_epi32(_mm256_hadd_epi32(acc[0], acc[1]),
                                            _mm256_hadd_epi32(acc[2], acc[3]));
        _mm_store_si128(reinterpret_cast<__m128i*>(sums + o),
                        _mm_add_epi32(_mm256_castsi256_si128(s), _mm256_extracti128_si256(s, 1)));
    }
    return tail(net, sums, outputBucket);
#endif
}

bool loadFromMemory(const unsigned char* data, size_t size, NetworkSmall& net) noexcept {
    if (size != FILE_BYTES) return false;
    size_t off = 0;
    const auto rd = [&](void* dst, size_t bytes) noexcept {
        std::memcpy(dst, data + off, bytes);
        off += bytes;
    };
    rd(net.l0w, sizeof(net.l0w));
    rd(net.l0b, sizeof(net.l0b));
    rd(net.l1w, sizeof(net.l1w));
    rd(net.l1b, sizeof(net.l1b));
    rd(net.l2w, sizeof(net.l2w));
    rd(net.l2b, sizeof(net.l2b));

    static const char kTag[] = "bullet";
    for (size_t i = PAYLOAD_BYTES; i < size; ++i) {
        if (data[i] != static_cast<unsigned char>(kTag[(i - PAYLOAD_BYTES) % 6])) return false;
    }

    for (int b = 0; b < OUTPUT_BUCKETS; ++b)
        for (int o = 0; o < L1_SIZE; ++o)
            for (int pos = 0; pos < HIDDEN; ++pos) {
                const int block = pos & ~31;
                net.l1wP[b][o][pos] = net.l1w[b][o][block + Deep::PACKUS_MAP[pos - block]];
            }
    return true;
}

bool loadNetwork(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<unsigned char> blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto net = std::make_unique<NetworkSmall>();
    if (!loadFromMemory(blob.data(), blob.size(), *net)) {
        std::cout << "info string SmallEvalFile error: cannot load '" << path << "' (expected "
                  << FILE_BYTES << " bytes, " << HIDDEN << " -> " << L1_SIZE << " -> 1)\n";
        return false;
    }
    owned = std::move(net);
    activeSmallNetwork = owned.get();
    ++smallNetworkGeneration;
    std::cout << "info string SmallEvalFile: small network (" << HIDDEN << " -> " << L1_SIZE << " -> 1)\n";
    return true;
}

int32_t evaluate(const chess::Board& b) noexcept {
    b.ensureSmallAccumulatorClean();
    const SmallAccumulator& acc = b.smallAccumulator();
    const int stm = chess::Board::colorToIndex(b.getActiveColor());
    const int bucket = (std::popcount(b.getPiecesBitMap()) - 2) / 4;   // as NNUE::evaluate
    return forwardSimd(*activeSmallNetwork, acc.v[stm], acc.v[stm ^ 1], bucket);
}

} // namespace Small
} // namespace NNUE
