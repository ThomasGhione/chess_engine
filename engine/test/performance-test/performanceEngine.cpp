// Performance tests for the chess engine

#include "../../engine.hpp"
#include "../../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite performanceEngineSuite = [] {
  using namespace ut;

  "performance searchPosition depth 6"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 6;

    auto start = std::chrono::high_resolution_clock::now();
    e.search(e.depth);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Attesa che la ricerca venga completata in meno di 70 milli
    printf("Depth 6 search completed in %lu ms\n", duration);
    printf("Nodes searched: %lu\n", engine::Engine::nodesSearched);
    expect(duration < 70);
  };

    
  "avg performance searchPosition depth 6 over 20 runs"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 6;

    constexpr int runs = 20;
    int64_t totalDuration = 0;

    // plays against itself for "runs" moves
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        e.search(e.depth);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("Run %d completed in %lu ms\n", i + 1, duration);
        totalDuration += duration;
    }

    double avgDuration = static_cast<double>(totalDuration) / runs;

    // Attesa che la ricerca media venga completata in meno di 200 millisecondi
    printf("Average Depth 6 search time over %d runs: %.2f ms\n", runs, avgDuration);
    expect(avgDuration < 500);
  };

  "banchmark all evaluation helper functions"_test = []{
    engine::Engine e = engine::Engine("r3kbnr/pppbpppp/4q3/8/1n6/P1NPB3/1PP1NPPP/R2QKB1R b KQkq - 0 1");
    
    const uint64_t whitePawns = e.board.pawns_bb[0];
    const uint64_t blackPawns = e.board.pawns_bb[1];

    constexpr int EVAL_HELPER_FUNCTIONS_ITERATIONS = 10'000'000;

    // evalMobility, evalHangingPieces, evalTrappedPieces
    // are now private - benchmarks removed

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evalPawnStructure(e.board.pawns_bb[0], e.board.pawns_bb[1]);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
    printf("Pawn structure evaluation took %lu ns\n", duration2);

    auto start3 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evalKingSafety(e.board, whitePawns, blackPawns);
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::nanoseconds>(end3 - start3).count();
    printf("King safety evaluation took %lu ns\n", duration3);

    auto start7 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evalRooks(e.board.rooks_bb[0], e.board.rooks_bb[1], whitePawns, blackPawns);
    auto end7 = std::chrono::high_resolution_clock::now();
    auto duration7 = std::chrono::duration_cast<std::chrono::nanoseconds>(end7 - start7).count();
    printf("Rooks evaluation took %lu ns\n", duration7);

    auto start8 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evalKingActivity(e.board, false);
    auto end8 = std::chrono::high_resolution_clock::now();
    auto duration8 = std::chrono::duration_cast<std::chrono::nanoseconds>(end8 - start8).count();
    printf("King activity evaluation took %lu ns\n", duration8);

    auto start9 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evalBadKingPosition(e.board);
    auto end9 = std::chrono::high_resolution_clock::now();
    auto duration9 = std::chrono::duration_cast<std::chrono::nanoseconds>(end9 - start9).count();
    printf("Bad king position evaluation took %lu ns\n", duration9);

    auto start10 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evalEndgameKingActivity(e.board);
    auto end10 = std::chrono::high_resolution_clock::now();
    auto duration10 = std::chrono::duration_cast<std::chrono::nanoseconds>(end10 - start10).count();
    printf("Endgame king activity evaluation took %lu ns\n", duration10);

    // evalPassedPawnScaling() removed - scaling is now integrated into evalPawnStructure()

    auto start12 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evalBadBishop(e.board.bishops_bb[0], whitePawns, 0);
    auto end12 = std::chrono::high_resolution_clock::now();
    auto duration12 = std::chrono::duration_cast<std::chrono::nanoseconds>(end12 - start12).count();
    printf("Bad bishop evaluation took %lu ns\n", duration12);

    expect(true);
    printf("Benchmark completed.");
  };

};
