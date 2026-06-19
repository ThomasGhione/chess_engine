// Deterministic single-thread node-count + NPS bench.
//
// Purpose: behavior-preservation signal for refactors of the search hot path.
// Node counts at fixed depth on a fixed position set are deterministic when the
// search runs single-threaded (no YBWC nondeterminism) and are independent of
// optimization flags, so this binary can be linked against the existing object
// files for a fast correctness check, or compiled with prod flags for NPS.
//
//   total nodes MUST be identical before/after a behavior-preserving change.
//
// Build (fast, links current output/*.o):
//   g++ -std=c++23 -fopenmp -march=native -O2 tests/nodebench.cpp \
//       output/engine/**/*.o output/board/*.o output/uci/*.o output/driver/*.o \
//       output/engine/syzygy/tbprobe.o -o tests/nodebench

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../board/piece.hpp"
#include "../engine/engine.hpp"

int main(int argc, char** argv) {
    pieces::initMagicBitboards();

    // Fixed depth (override with argv[1]); single-thread for determinism.
    const uint64_t depth = (argc > 1) ? std::stoull(argv[1]) : 11;

    // A spread of opening / middlegame / tactical / endgame positions.
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 4 3",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "8/8/8/2k5/2pP4/8/B7/4K3 b - d3 0 1",
        "2rq1rk1/pp1bppbp/2np1np1/8/3NP3/1BN1BP2/PPPQ2PP/2KR3R w - - 0 1",
        "8/3k4/8/8/3K4/8/3P4/8 w - - 0 1",
    };

    uint64_t totalNodes = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < fens.size(); ++i) {
        engine::Engine e;
        e.openingEnabled.store(false, std::memory_order_relaxed);
        e.searchRuntime.maxThreads = 1;        // no YBWC -> deterministic
        e.board = chess::Board(fens[i]);
        e.searchUCI(depth);
        totalNodes += e.nodesSearched;
        std::printf("pos %zu  nodes %12lu\n", i + 1, (unsigned long)e.nodesSearched);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    const double ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    const double nps = (ms > 0.0) ? (totalNodes / (ms / 1000.0)) : 0.0;

    std::printf("--------------------------------------\n");
    std::printf("depth %lu  TOTAL nodes %lu  time %.1f ms  nps %.0f\n",
                (unsigned long)depth, (unsigned long)totalNodes, ms, nps);
    return 0;
}
