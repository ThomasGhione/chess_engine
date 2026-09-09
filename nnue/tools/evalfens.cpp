// Bulk static evaluation with the single-layer net: one FEN per line on stdin,
// "eval<TAB>bucket<TAB>fen" on stdout. The coverage audit needs it: measuring
// where the net is wrong takes tens of thousands of positions, and sanity.rs
// takes its FENs from argv, a handful at a time.
//
//   g++ -std=c++23 -O2 -march=native nnue/tools/evalfens.cpp -o /tmp/evalfens
//   /tmp/evalfens <net.bin> < fens.txt
//
// The forward is SCReLU over the two perspectives, sum, /QA, bias, *SCALE/
// (QA*QB). The accumulator comes from fen_accumulator.hpp, already shared with
// deepcheck and reorder.
//
// The single-layer net is ALSO read as a NetworkDeep for the l0 prefix alone:
// the two formats have identical l0w/l0b by construction, which is what the
// engine relies on (see the static_asserts in nnue.cpp).
//
// The single-layer LAYOUT is declared here rather than included: the engine
// dropped its shallow-net support, so this tool is now the only reader of the
// format and owns its definition. NNUE::Network in network.hpp is the l0 prefix
// alone, which is all the engine needs.

#include "fen_accumulator.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

// Single-layer quantised.bin: (768x4kb_hm -> 1024)x2 -> 8, SCReLU, QA=255 QB=64.
constexpr int     INPUTS        = 768;
constexpr int     HIDDEN        = 1024;
constexpr int     INPUT_BUCKETS = 4;

// Every net in this format has four king buckets, and the accumulator this tool
// borrows from fen_accumulator.hpp uses the map compiled into network.hpp. On a
// branch whose map has a different bucket count the two disagree and the reads
// run past the file, so refuse to build rather than print wrong evaluations.
static_assert(NNUE::INPUT_BUCKETS == INPUT_BUCKETS,
              "single-layer nets are all 4-bucket: this tool is for a 4-bucket build");
constexpr int     OUTPUT_BUCKETS = 8;
constexpr int32_t QA = 255;
constexpr int32_t QB = 64;
constexpr int32_t SCALE = 400;

struct alignas(64) ShallowNet {
    int16_t featureWeights[INPUT_BUCKETS * INPUTS][HIDDEN]; // l0w (QA)
    int16_t featureBias[HIDDEN];                            // l0b (QA)
    int16_t outputWeights[OUTPUT_BUCKETS][2][HIDDEN];       // l1w (QB)
    int16_t outputBias[OUTPUT_BUCKETS];                     // l1b (QA*QB)
};

constexpr size_t PAYLOAD_BYTES =
    sizeof(int16_t) * (INPUT_BUCKETS * INPUTS * HIDDEN + HIDDEN
                       + OUTPUT_BUCKETS * 2 * HIDDEN + OUTPUT_BUCKETS);
static_assert(PAYLOAD_BYTES == 6326288);

int32_t forwardHalf(const int16_t* acc, const int16_t* w) {
    int32_t s = 0;
    for (int i = 0; i < HIDDEN; ++i) {
        const int32_t t = std::clamp<int32_t>(acc[i], 0, QA);
        s += t * t * static_cast<int32_t>(w[i]);
    }
    return s;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: evalfens <net.bin> < fens.txt\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary | std::ios::ate);
    if (!in) { std::fprintf(stderr, "cannot open: %s\n", argv[1]); return 1; }
    const auto size = static_cast<size_t>(in.tellg());
    if (size < PAYLOAD_BYTES) {
        std::fprintf(stderr, "size %zu: not a single-layer net (expected >= %zu)\n",
                     size, PAYLOAD_BYTES);
        return 1;
    }
    in.seekg(0);

    auto buf = std::make_unique<ShallowNet>();
    if (!in.read(reinterpret_cast<char*>(buf.get()),
                 static_cast<std::streamsize>(PAYLOAD_BYTES))) {
        std::fprintf(stderr, "short read\n");
        return 1;
    }
    const ShallowNet& net = *buf;
    // A view on the l0 prefix alone, which both formats share byte for byte.
    const auto& deepView = *reinterpret_cast<const NNUE::Deep::NetworkDeep*>(buf.get());

    std::vector<int16_t> accUs, accThem;
    std::string fen;
    while (std::getline(std::cin, fen)) {
        if (fen.empty()) continue;
        int bucket = 0;
        if (!NNUE::Deep::tools::accumulate(deepView, fen, accUs, accThem, bucket)) {
            std::printf("NA\tNA\t%s\n", fen.c_str());
            continue;
        }
        int32_t out = forwardHalf(accUs.data(), net.outputWeights[bucket][0])
                    + forwardHalf(accThem.data(), net.outputWeights[bucket][1]);
        out /= QA;
        out += net.outputBias[bucket];
        out = out * SCALE / (QA * QB);
        std::printf("%d\t%d\t%s\n", out, bucket, fen.c_str());
    }
    return 0;
}
