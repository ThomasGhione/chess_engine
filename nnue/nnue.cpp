#include "nnue.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#include "../board/board.hpp"
#include "accumulator.hpp"
#include "network.hpp"

namespace NNUE {

const Network* activeNetwork = nullptr;

namespace {

std::unique_ptr<Network> ownedNetwork;

// The AVX2 forward multiplies clamped activations (<= QA) by output weights in
// int16: |w| must stay <= 32767/QA or mullo_epi16 wraps. bullet's AdamW clips
// weights to [-1.98, 1.98] (so |w| <= ~127 at QB=64); reject anything looser.
constexpr int32_t MAX_OUTPUT_WEIGHT = 32767 / QA; // 128

// One half of the output layer: sum of screlu(acc[i]) * w[i].
// Scalar version is the reference (identical to trainer/src/bin/sanity.rs).
int32_t forwardHalf(const int16_t* __restrict acc, const int16_t* __restrict w) noexcept {
#if defined(__AVX2__)
    const __m256i zero = _mm256_setzero_si256();
    const __m256i qa = _mm256_set1_epi16(static_cast<int16_t>(QA));
    __m256i sum = _mm256_setzero_si256();
    for (int i = 0; i < HIDDEN; i += 16) {
        const __m256i a = _mm256_load_si256(reinterpret_cast<const __m256i*>(acc + i));
        const __m256i t = _mm256_min_epi16(_mm256_max_epi16(a, zero), qa);
        const __m256i wi = _mm256_load_si256(reinterpret_cast<const __m256i*>(w + i));
        // (t*w) fits i16 thanks to MAX_OUTPUT_WEIGHT; madd gives (t*w)*t in i32.
        const __m256i p = _mm256_mullo_epi16(t, wi);
        sum = _mm256_add_epi32(sum, _mm256_madd_epi16(p, t));
    }
    const __m128i lo = _mm256_castsi256_si128(sum);
    const __m128i hi = _mm256_extracti128_si256(sum, 1);
    __m128i s = _mm_add_epi32(lo, hi);
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, 0b01001110));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, 0b10110001));
    return _mm_cvtsi128_si32(s);
#else
    int32_t s = 0;
    for (int i = 0; i < HIDDEN; ++i) {
        const int32_t t = std::clamp<int32_t>(acc[i], 0, QA);
        s += t * t * static_cast<int32_t>(w[i]);
    }
    return s;
#endif
}

} // namespace

bool loadNetwork(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        std::cout << "info string EvalFile error: cannot open '" << path << "'\n";
        return false;
    }
    const auto fileSize = static_cast<size_t>(in.tellg());
    if (fileSize < NETWORK_PAYLOAD_BYTES || fileSize % 64 != 0) {
        std::cout << "info string EvalFile error: size " << fileSize
                  << " (expected " << NETWORK_PAYLOAD_BYTES << " padded to 64)\n";
        return false;
    }
    in.seekg(0);

    auto net = std::make_unique<Network>();
    if (!in.read(reinterpret_cast<char*>(net.get()),
                 static_cast<std::streamsize>(NETWORK_PAYLOAD_BYTES))) {
        std::cout << "info string EvalFile error: short read\n";
        return false;
    }

    // bullet pads quantised.bin to 64 bytes with the repeating ASCII string
    // "bullet"; anything else means the layout drifted.
    std::vector<char> tail(fileSize - NETWORK_PAYLOAD_BYTES);
    static constexpr char SIG[6] = {'b', 'u', 'l', 'l', 'e', 't'};
    if (in.read(tail.data(), static_cast<std::streamsize>(tail.size()))) {
        for (size_t i = 0; i < tail.size(); ++i) {
            if (tail[i] != SIG[i % 6]) {
                std::cout << "info string EvalFile error: bad padding signature (layout drift?)\n";
                return false;
            }
        }
    }

    for (const int16_t w : net->outputWeights[0]) {
        if (std::abs(static_cast<int32_t>(w)) > MAX_OUTPUT_WEIGHT) {
            std::cout << "info string EvalFile error: output weight " << w
                      << " exceeds AVX2-safe bound " << MAX_OUTPUT_WEIGHT << "\n";
            return false;
        }
    }
    for (const int16_t w : net->outputWeights[1]) {
        if (std::abs(static_cast<int32_t>(w)) > MAX_OUTPUT_WEIGHT) {
            std::cout << "info string EvalFile error: output weight " << w
                      << " exceeds AVX2-safe bound " << MAX_OUTPUT_WEIGHT << "\n";
            return false;
        }
    }

    ownedNetwork = std::move(net);
    activeNetwork = ownedNetwork.get();
    return true;
}

bool networkLoaded() noexcept {
    return activeNetwork != nullptr;
}

int32_t evaluate(const chess::Board& b) noexcept {
    const Network& net = *activeNetwork;
    const NNUE::Accumulator& acc = b.nnueAccumulator;
    const int stm = chess::Board::colorToIndex(b.getActiveColor()); // 0 = white

    int32_t out = forwardHalf(acc.v[stm], net.outputWeights[0])
                + forwardHalf(acc.v[stm ^ 1], net.outputWeights[1]);
    out /= QA;
    out += net.outputBias;
    return out * SCALE / (QA * QB);
}

} // namespace NNUE
