// At the moment it appears done.

#include "../engine.hpp"
#include "../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite engineSuite = [] {
  using namespace ut;

  /*
  "getMaterialDelta FAST vs NORMAL vs SLOW"_test = []{
    engine::Engine e = engine::Engine();

    // Creare board con fen:
    // r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2

    chess::Board testBoard("r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
    
    int64_t deltaFast = 0;
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      deltaFast += e.getMaterialDeltaFAST(testBoard);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    printf("Fast material delta calculated in: %lu\n", duration1);

    int64_t deltaNormal = 0;
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) {
      deltaNormal += e.getMaterialDelta(testBoard);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();
    printf("Normal material delta calculated in: %lu\n", duration2);

    expect(duration1 < duration2);
  };
*/
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
/*
  "calculate 1mln nodes"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 6;

    constexpr uint64_t targetNodes = 1'000'000;
    e.nodesSearched = 0;

    auto start = std::chrono::high_resolution_clock::now();
    while (e.nodesSearched < targetNodes) {
        e.search(e.depth);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Attesa che la ricerca di 1 milione di nodi venga completata in meno di 5 secondi
    printf("1 million nodes searched in %lu ms\n", duration);
    expect(duration < 5000);
  };
*/

  "is engine generating & sorting castling rights correctly"_test = []{
    engine::Engine e = engine::Engine("8/6pp/6p1/6pk/2p1p1p1/1pPpPpPp/1P1P1P1P/3NK2R w K - 0 1");
    e.depth = 3;

    auto moves = e.generateLegalMoves(e.board);
    expect(moves.size == 4) 
      << "Expected 4 legal moves, got " << moves.size << '\n';  // Only king-side castling and king moves are legal


    int castleMoveIndex = -1;

    castleMoveIndex = std::distance(moves.begin(), 
      std::find_if(moves.begin(), moves.end(), [](const chess::Board::Move& m) {
        return (m.from == chess::Coords("e1") && m.to == chess::Coords("g1"));
      })
    );

    auto sortedMoves = e.sortLegalMoves(moves, 0, e.board, true);
    expect(sortedMoves[0].move.from == chess::Coords("e1") && sortedMoves[0].move.to == chess::Coords("g1"))
      << "Expected castling move to be prioritized, but got move from " << sortedMoves[0].move.from.toString() << sortedMoves[0].move.to.toString() << '\n'
      << "with score " << sortedMoves[0].score << '\n'
      << "Castling move at index:" << castleMoveIndex << '\n';
    
    for (auto idx : {0, 1, 2, 3}) {
      printf("Move %d: from %s to %s with score %ld\n", idx + 1,
             sortedMoves[idx].move.from.toString().c_str(),
             sortedMoves[idx].move.to.toString().c_str(),
             sortedMoves[idx].score);
    }
    
  };

  "is engine generating pin correctly"_test = []{
    engine::Engine e = engine::Engine("8/r5p1/5npk/6p1/6P1/6PR/6PK/8 b - - 0 1");
    e.depth = 2;

    auto moves = e.generateLegalMoves(e.board);

    expect(moves.size == 1) // Only knight move is legal
      << "Expected 1 legal move, got " << moves.size << '\n'
      << moves[0].from.toString() << moves[0].to.toString() << '\n'
      << moves[1].from.toString() << moves[1].to.toString() << '\n';
  };

  "critical position 1"_test = []{
    engine::Engine e = engine::Engine("r3kbnr/pppbpppp/4q3/8/1n6/P1NPB3/1PP1NPPP/R2QKB1R b KQkq - 0 1");
    e.depth = 6;

    auto moves = e.generateLegalMoves(e.board);

    // for (const auto& m : moves) {
    //   printf("Legal move from %s to %s\n", m.from.toString().c_str(), m.to.toString().c_str());
    // }

    int64_t evaluation = e.evaluate(e.board);
    printf("Evaluation: %ld\n", evaluation);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    expect(moves.size == 46) // Expected number of legal moves
      << "Expected 46 legal moves, got " << moves.size << '\n';

    expect(bestMove.from != chess::Coords("b4") && bestMove.to != chess::Coords("c2"))
      << "Expected bestMove not to be b4 c2, got" << bestMove.from.toString() << bestMove.to.toString() << '\n';
  };

  "critical position 2"_test = []{
    engine::Engine e = engine::Engine("r3kbnr/pppbpppp/4q3/8/8/P1NPB3/1Pn1NPPP/R2QKB1R w KQkq - 0 1");
    e.depth = 6;

    auto moves = e.generateLegalMoves(e.board);

    int64_t evaluation = e.evaluate(e.board);
    printf("Evaluation: %ld\n", evaluation);

    // for (const auto& m : moves) {
    //   printf("Legal move from %s to %s\n", m.from.toString().c_str(), m.to.toString().c_str());
    // }


    expect(moves.size == 2) // Expected number of legal moves
      << "Expected 2 legal moves, got " << moves.size << '\n';
  };

  "banchmark all evaluation helper functions"_test = []{
    engine::Engine e = engine::Engine("r3kbnr/pppbpppp/4q3/8/1n6/P1NPB3/1PP1NPPP/R2QKB1R b KQkq - 0 1");
    
    const uint64_t whitePawns = e.board.pawns_bb[0];
    const uint64_t blackPawns = e.board.pawns_bb[1];

    constexpr int EVAL_HELPER_FUNCTIONS_ITERATIONS = 10'000'000;
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateMobilityFast(e.board, e.board.getPiecesBitMap());
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - start1).count();
    printf("Mobility evaluation took %lu ns\n", duration1);

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluatePawnStructureFast(e.board.pawns_bb[0], e.board.pawns_bb[1]);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
    printf("Pawn structure evaluation took %lu ns\n", duration2);

    auto start3 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateKingSafetyFast(e.board, whitePawns, blackPawns);
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::nanoseconds>(end3 - start3).count();  
    printf("King safety evaluation took %lu ns\n", duration3);

    auto start4 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateHangingPiecesFast(e.board, e.board.getPiecesBitMap());
    auto end4 = std::chrono::high_resolution_clock::now();
    auto duration4 = std::chrono::duration_cast<std::chrono::milliseconds>(end4 - start4).count();  
    printf("Hanging pieces evaluation took %lu ms\n", duration4);

    auto start5 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateTrappedPiecesFast(e.board, e.board.getPiecesBitMap());
    auto end5 = std::chrono::high_resolution_clock::now();
    auto duration5 = std::chrono::duration_cast<std::chrono::nanoseconds>(end5 - start5).count();
    printf("Trapped pieces evaluation took %lu ns\n", duration5);

    auto start6 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateMobilityFast(e.board, e.board.getPiecesBitMap());
    auto end6 = std::chrono::high_resolution_clock::now();
    auto duration6 = std::chrono::duration_cast<std::chrono::nanoseconds>(end6 - start6).count();
    printf("Mobility evaluation took %lu ns\n", duration6);

    auto start7 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateRooksFast(e.board.rooks_bb[0], e.board.rooks_bb[1], whitePawns, blackPawns);
    auto end7 = std::chrono::high_resolution_clock::now();
    auto duration7 = std::chrono::duration_cast<std::chrono::nanoseconds>(end7 - start7).count();
    printf("Rooks evaluation took %lu ns\n", duration7);

    auto start8 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateKingActivityFast(e.board, false);
    auto end8 = std::chrono::high_resolution_clock::now();
    auto duration8 = std::chrono::duration_cast<std::chrono::nanoseconds>(end8 - start8).count();
    printf("King activity evaluation took %lu ns\n", duration8);

    auto start9 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateBadKingPositionFast(e.board);
    auto end9 = std::chrono::high_resolution_clock::now();
    auto duration9 = std::chrono::duration_cast<std::chrono::nanoseconds>(end9 - start9).count();
    printf("Bad king position evaluation took %lu ns\n", duration9);

    auto start10 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateEndgameKingActivityFast(e.board);
    auto end10 = std::chrono::high_resolution_clock::now();
    auto duration10 = std::chrono::duration_cast<std::chrono::nanoseconds>(end10 - start10).count();
    printf("Endgame king activity evaluation took %lu ns\n", duration10);

    auto start11 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluatePassedPawnScalingFast(whitePawns, blackPawns);
    auto end11 = std::chrono::high_resolution_clock::now();
    auto duration11 = std::chrono::duration_cast<std::chrono::nanoseconds>(end11 - start11).count();
    printf("Passed pawn scaling evaluation took %lu ns\n", duration11);

    auto start12 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < EVAL_HELPER_FUNCTIONS_ITERATIONS; ++i) e.evaluateBadBishopFast(e.board.bishops_bb[0], whitePawns, 0);
    auto end12 = std::chrono::high_resolution_clock::now();
    auto duration12 = std::chrono::duration_cast<std::chrono::nanoseconds>(end12 - start12).count();
    printf("Bad bishop evaluation took %lu ns\n", duration12);

    expect(false) << "Benchmark completed.";
  }; 
  
};
