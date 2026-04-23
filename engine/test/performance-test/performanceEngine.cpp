// Performance tests for the chess engine

#include "../../engine.hpp"
#include "../../eval/evaluator.hpp"
#include "../../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite performanceEngineSuite = [] {
  using namespace ut;

  "performance searchPosition depth 11"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 11;

    auto start = std::chrono::high_resolution_clock::now();
    e.searchUCI(e.depth);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("Depth 11 search completed in %lu ms\n", duration);
    printf("Nodes searched: %lu\n", e.nodesSearched);
    expect(duration < 9000);
  };

    
  "avg performance searchPosition depth 10 over 20 runs"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 10;

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
    printf("Average Depth 10 search time over %d runs: %.2f ms\n", runs, avgDuration);
    expect(avgDuration < 700);
  };

  "benchmark all evaluation helper functions"_test = []{
    constexpr int EVAL_HELPER_FUNCTIONS_ITERATIONS = 10'000'000;
    constexpr std::array<const char*, 8> BENCH_FENS = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "r3kbnr/pppbpppp/4q3/8/1n6/P1NPB3/1PP1NPPP/R2QKB1R b KQkq - 0 1",
      "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 3",
      "rnbqk2r/ppp2ppp/4pn2/3p4/1b1P4/2N1PN2/PPP2PPP/R1BQKB1R w KQkq - 2 5",
      "r2q1rk1/pp2bppp/2np1n2/2p1p3/2P1P3/2NP1N2/PP2BPPP/R1BQ1RK1 w - - 0 9",
      "8/2p5/3p2k1/2P1p1p1/4P3/3P1K2/8/8 w - - 0 40",
      "4rrk1/pp3ppp/2n1bn2/2bp4/3P4/2P1PN2/PP1NBPPP/R2QR1K1 w - - 4 13",
      "r3k2r/pppq1ppp/2npbn2/3Np3/2P1P3/2N1B3/PP2QPPP/R3KB1R w KQkq - 2 10",
    };

    constexpr std::array<const char *, 8> ENDGAME_BENCH_FENS = {
      "8/p7/4k3/1R6/2K5/8/P7/8 w - - 0 1",
      "8/8/8/3R4/8/3k4/8/1R3K2 w - - 0 1",
      "8/8/1r6/8/8/3k4/8/1R3K2 w - - 0 1",
      "8/8/8/3R4/8/3k4/8/1R1R1K2 w - - 0 1", // double rook
      "r1b1k2r/pppq1ppp/2npbn2/3Np3/2P1P3/2N1B3/PP2QPPP/R3KB1R w KQkq - 2 10",
      "rnbqk2r/ppp2ppp/4pn2/3p4/1b1P4/2N1PN2/PPP2PPP/R1BQKB1R w KQkq - 2 5",
      "r2q1rk1/pp2bppp/2np1n2/2p1p3/2P1P3/2NP1N2/PP2BPPP/R1BQ1RK1 w - - 0 9",
      "8/2p5/3p2k1/2P1p1p1/4P3/3P1K2/8/8 w - - 0 40"
    };

    struct BenchPosition {
      chess::Board board{};
      uint64_t whitePawns = 0ULL;
      uint64_t blackPawns = 0ULL;
      uint64_t whiteRooks = 0ULL;
      uint64_t blackRooks = 0ULL;
      uint64_t whiteBishops = 0ULL;
    };

    auto preparePositions = []<size_t N>(const std::array<const char*, N>& fens) {
      std::array<BenchPosition, N> prepared{};
      for (size_t i = 0; i < N; ++i) {
        prepared[i].board = chess::Board(fens[i]);
        prepared[i].whitePawns = prepared[i].board.pawns_bb[0];
        prepared[i].blackPawns = prepared[i].board.pawns_bb[1];
        prepared[i].whiteRooks = prepared[i].board.rooks_bb[0];
        prepared[i].blackRooks = prepared[i].board.rooks_bb[1];
        prepared[i].whiteBishops = prepared[i].board.bishops_bb[0];
      }
      return prepared;
    };

    const auto benchPositions = preparePositions(BENCH_FENS);
    const auto benchEndgamePositions = preparePositions(ENDGAME_BENCH_FENS);

    static_assert((BENCH_FENS.size() & (BENCH_FENS.size() - 1)) == 0, "BENCH_FENS size must be a power of two");
    static_assert((ENDGAME_BENCH_FENS.size() & (ENDGAME_BENCH_FENS.size() - 1)) == 0, "ENDGAME_BENCH_FENS size must be a power of two");
    constexpr size_t BENCH_MASK = BENCH_FENS.size() - 1;
    constexpr size_t ENDGAME_BENCH_MASK = ENDGAME_BENCH_FENS.size() - 1;

    auto benchPosAt = [&](int i, bool isEndgame = false) -> const BenchPosition& {
      if (isEndgame) {
        return benchEndgamePositions[static_cast<size_t>(i) & ENDGAME_BENCH_MASK];
      }
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

    auto startPawnFileStats = std::chrono::high_resolution_clock::now();
    int64_t pawnFileStatsSink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i);
      const auto stats = engine::Evaluator::evalPawnFileStats(pos.whitePawns, pos.blackPawns);
      pawnFileStatsSink += stats.doubledScore + stats.islandScore + stats.whiteIslands - stats.blackIslands;
    }
    auto endPawnFileStats = std::chrono::high_resolution_clock::now();
    auto durationPawnFileStats = std::chrono::duration_cast<std::chrono::milliseconds>(endPawnFileStats - startPawnFileStats).count();
    benchmarkSink ^= pawnFileStatsSink;
    printf("Pawn file stats evaluation took %lld ms\n", static_cast<long long>(durationPawnFileStats));

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

    auto start13 = std::chrono::high_resolution_clock::now();
    int64_t doubleRookSink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i + 6, true);
      int wR = std::popcount(pos.whiteRooks);
      int bR = std::popcount(pos.blackRooks);
      doubleRookSink += engine::Evaluator::evalDoubleRookEndgameSide(pos.board, 0, wR, bR);
      doubleRookSink += engine::Evaluator::evalDoubleRookEndgameSide(pos.board, 1, wR, bR);
    }
    auto end13 = std::chrono::high_resolution_clock::now();
    auto duration13 = std::chrono::duration_cast<std::chrono::milliseconds>(end13 - start13).count();
    benchmarkSink ^= doubleRookSink;
    printf("Double rook endgame evaluation took %lld ms\n", static_cast<long long>(duration13));

    auto start14 = std::chrono::high_resolution_clock::now();
    int64_t rookPressureSink = 0;
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      const auto& pos = benchPosAt(i + 7, true);
      int wR = std::popcount(pos.whiteRooks);
      int bR = std::popcount(pos.blackRooks);
      rookPressureSink += engine::Evaluator::evalRookEndgamePressureSide(pos.board, 0, wR, bR);
      rookPressureSink += engine::Evaluator::evalRookEndgamePressureSide(pos.board, 1, wR, bR);
    }
    auto end14 = std::chrono::high_resolution_clock::now();
    auto duration14 = std::chrono::duration_cast<std::chrono::milliseconds>(end14 - start14).count();
    benchmarkSink ^= rookPressureSink;
    printf("Rook endgame pressure evaluation took %lld ms\n", static_cast<long long>(duration14));

    printf("Benchmark sink: %lld\n", static_cast<long long>(benchmarkSink));
    
    expect(true);
    printf("Benchmark completed.\n");
  };
};
