// Performance tests for the chess engine

#include "../../engine.hpp"
#include "../../eval/evaluator.hpp"
#include "../../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite performanceEngineSuite = [] {
  using namespace ut;

  "performance searchPosition depth 10"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 10;

    auto start = std::chrono::high_resolution_clock::now();
    e.searchUCI(e.depth);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Attesa che la ricerca venga completata in meno di 300 milli
    printf("Depth 10 search completed in %lu ms\n", duration);
    printf("Nodes searched: %lu\n", e.nodesSearched);
    expect(duration < 300);
  };

    
  "avg performance searchPosition depth 8 over 20 runs"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 8;

    constexpr int runs = 20;
    int64_t totalDuration = 0;

    // plays against itself for "runs" moves
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        e.searchUCI(e.depth);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("Run %d completed in %lu ms\n", i + 1, duration);
        totalDuration += duration;
    }

    double avgDuration = static_cast<double>(totalDuration) / runs;

    // Attesa che la ricerca media venga completata in meno di 500 millisecondi
    printf("Average Depth 8 search time over %d runs: %.2f ms\n", runs, avgDuration);
    expect(avgDuration < 500);
  };

  "banchmark all evaluation helper functions"_test = []{
    constexpr int EVAL_HELPER_FUNCTIONS_ITERATIONS = 10'000'000;
    constexpr std::array<const char*, 8> BENCH_FENS = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "r3kbnr/pppbpppp/4q3/8/1n6/P1NPB3/1PP1NPPP/R2QKB1R b KQkq - 0 1",
      "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 3",
      "rnbqk2r/ppp2ppp/4pn2/3p4/1b1P4/2N1PN2/PPP2PPP/R1BQKB1R w KQkq - 2 5",
      "r2q1rk1/pp2bppp/2np1n2/2p1p3/2P1P3/2NP1N2/PP2BPPP/R1BQ1RK1 w - - 0 9",
      "8/2p5/3p2k1/2P1p1p1/4P3/3P1K2/8/8 w - - 0 40",
      "4rrk1/pp3ppp/2n1bn2/2bp4/3P4/2P1PN2/PP1NBPPP/R2QR1K1 w - - 4 13",
      "r3k2r/pppq1ppp/2npbn2/3Np3/2P1P3/2N1B3/PP2QPPP/R3KB1R w KQkq - 2 10"
    };

    struct BenchPosition {
      chess::Board board{};
      uint64_t whitePawns = 0ULL;
      uint64_t blackPawns = 0ULL;
      uint64_t whiteRooks = 0ULL;
      uint64_t blackRooks = 0ULL;
      uint64_t whiteBishops = 0ULL;
    };

    const auto benchPositions = [&] {
      std::array<BenchPosition, BENCH_FENS.size()> prepared{};
      for (size_t i = 0; i < BENCH_FENS.size(); ++i) {
        prepared[i].board = chess::Board(BENCH_FENS[i]);
        prepared[i].whitePawns = prepared[i].board.pawns_bb[0];
        prepared[i].blackPawns = prepared[i].board.pawns_bb[1];
        prepared[i].whiteRooks = prepared[i].board.rooks_bb[0];
        prepared[i].blackRooks = prepared[i].board.rooks_bb[1];
        prepared[i].whiteBishops = prepared[i].board.bishops_bb[0];
      }
      return prepared;
    }();

    static_assert((BENCH_FENS.size() & (BENCH_FENS.size() - 1)) == 0, "BENCH_FENS size must be a power of two");
    constexpr size_t BENCH_MASK = BENCH_FENS.size() - 1;
    auto benchPosAt = [&](int i) -> const BenchPosition& {
      return benchPositions[static_cast<size_t>(i) & BENCH_MASK];
    };

    // evalMobility, evalHangingPieces, evalTrappedPieces
    // are now private - benchmarks removed

    int64_t benchmarkSink = 0;

    auto start2 = std::chrono::high_resolution_clock::now();
    int64_t pawnSink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i);
      pawnSink += engine::Evaluator::evalPawnStructure(pos.whitePawns, pos.blackPawns, static_cast<bool>(i & 1));
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();
    benchmarkSink ^= pawnSink;
    printf("Pawn structure evaluation took %lld ms\n", static_cast<long long>(duration2));

    auto start3 = std::chrono::high_resolution_clock::now();
    int64_t kingSafetySink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i + 1);
      kingSafetySink += engine::Evaluator::evalKingSafety(pos.board, pos.whitePawns, pos.blackPawns);
    }
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(end3 - start3).count();
    benchmarkSink ^= kingSafetySink;
    printf("King safety evaluation took %lld ms\n", static_cast<long long>(duration3));

    auto start7 = std::chrono::high_resolution_clock::now();
    int64_t rooksSink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i + 2);
      rooksSink += engine::Evaluator::evalRooks(pos.whiteRooks, pos.blackRooks, pos.whitePawns, pos.blackPawns);
    }
    auto end7 = std::chrono::high_resolution_clock::now();
    auto duration7 = std::chrono::duration_cast<std::chrono::milliseconds>(end7 - start7).count();
    benchmarkSink ^= rooksSink;
    printf("Rooks evaluation took %lld ms\n", static_cast<long long>(duration7));

    auto start8 = std::chrono::high_resolution_clock::now();
    int64_t kingActivitySink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i + 3);
      kingActivitySink += engine::Evaluator::evalKingActivity(pos.board, false);
    }
    auto end8 = std::chrono::high_resolution_clock::now();
    auto duration8 = std::chrono::duration_cast<std::chrono::milliseconds>(end8 - start8).count();
    benchmarkSink ^= kingActivitySink;
    printf("King activity evaluation took %lld ms\n", static_cast<long long>(duration8));

    auto start10 = std::chrono::high_resolution_clock::now();
    int64_t endgameKingActivitySink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i + 4);
      endgameKingActivitySink += engine::Evaluator::evalEndgameKingActivity(pos.board);
    }
    auto end10 = std::chrono::high_resolution_clock::now();
    auto duration10 = std::chrono::duration_cast<std::chrono::milliseconds>(end10 - start10).count();
    benchmarkSink ^= endgameKingActivitySink;
    printf("Endgame king activity evaluation took %lld ms\n", static_cast<long long>(duration10));

    // evalPassedPawnScaling() removed - scaling is now integrated into evalPawnStructure()

    auto start12 = std::chrono::high_resolution_clock::now();
    int64_t badBishopSink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i + 5);
      badBishopSink += engine::Evaluator::evalBadBishop(pos.whiteBishops, pos.whitePawns, 0);
    }
    auto end12 = std::chrono::high_resolution_clock::now();
    auto duration12 = std::chrono::duration_cast<std::chrono::milliseconds>(end12 - start12).count();
    benchmarkSink ^= badBishopSink;
    printf("Bad bishop evaluation took %lld ms\n", static_cast<long long>(duration12));
    printf("Benchmark sink: %lld\n", static_cast<long long>(benchmarkSink));

    expect(true);
    printf("Benchmark completed.");
  };
};
