// At the moment it appears done.

#include "../engine.hpp"
#include "../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite engineSuite = [] {
  using namespace ut;

  "Default constructor"_test = []{
    engine::Engine e = engine::Engine();
    
    // Attesa una board vuota
  };

  "playGameVsHuman"_test = []{
    engine::Engine e = engine::Engine();

    // Scrivere mosse per matto dello scolaro:
    // e4-e5 Bc4-Kc6 Qf3-a6 Qxf3#
    // Attesa vittoria del bianco e interruzione gioco
  };
  
  "playGameVsHuman"_test = []{
    engine::Engine e = engine::Engine();

    // Scrivere mosse per matto dell'imbecille:
    // f4-e6 g4-Qh4#
    // Attesa vittoria del nero e interruzione gioco
  };

  "isMate"_test = []{
    // Creare board con fen:
    // k6R/6R1/8/8/8/8/8/2K5 w - - 0 1
    // Attesa isMate: true
  };
  
  "isMate"_test = []{
    // Creare board con fen:
    // k7/6R1/7R/8/8/8/8/2K5 w - - 0 1
    // Attesa isMate: false
  };


  "isMate"_test = []{
    // Creare board con fen:
    // rn1qkb1r/pp1bpppp/2pN1n2/8/8/5N2/PPPPQPPP/R1B1KB1R b KQkq - 4 6
    // Attesa isMate: true
  };

  "isMate"_test = []{
    // Creare board con fen:
    // 1k6/8/8/8/3rrr2/8/8/R3K2R w KQ - 0 1
    // Attesa isMate: false
  };

  "isMate"_test = []{
    // Creare board con fen:
    // k7/8/8/8/5n2/5nn1/8/7K w - - 0 1
    // Attesa isMate: true
  };

  "isMate"_test = []{
    // Creare board con fen:
    // r3k2r/pp1nQppp/5n2/3p4/1B6/P4N2/1Pq2PPP/RN2K2R b KQkq - 0 12
    // Attesa isMate: true
  };
  
  "isMate"_test = []{
    // Creare board con fen:
    // r1bk2nr/pppp1ppp/2n5/1N6/4q3/2P5/P1P1BPPP/R1BQ1RK1 b - - 2 9
    // Attesa isMate: false
  };

  "getMaterialDelta FAST vs NORMAL vs SLOW"_test = []{
    engine::Engine e = engine::Engine();

    // Creare board con fen:
    // r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2

    chess::Board testBoard("r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
    
    int64_t deltaFast = 0;
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
      deltaFast += e.getMaterialDeltaFAST(testBoard);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    printf("Fast material delta calculated in: %lu\n", duration1);

    int64_t deltaNormal = 0;
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
      deltaNormal += e.getMaterialDelta(testBoard);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();
    printf("Normal material delta calculated in: %lu\n", duration2);

    
    expect(duration1 < duration2);
  };



  "performance searchPosition depth 4"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 4;

    auto start = std::chrono::high_resolution_clock::now();
    e.search(e.depth);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Attesa che la ricerca venga completata in meno di 500 milli
    printf("Depth 4 search completed in %lu ms\n", duration);
    printf("Nodes searched: %lu\n", engine::Engine::nodesSearched);
    expect(duration < 500);
  };

  /*
  "performance searchPosition depth 6"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 6;

    auto start = std::chrono::high_resolution_clock::now();
    e.search(e.depth);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Attesa che la ricerca venga completata in meno di 15 secondi
    printf("Depth 6 search completed in %lu ms\n", duration);
    printf("Nodes searched: %lu\n", engine::Engine::nodesSearched);
    expect(duration < 2000);
  };
  */

  "avg performance searchPosition depth 4 over 20 runs"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 4;

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

    // Attesa che la ricerca media venga completata in meno di 500 millisecondi
    printf("Average Depth 4 search time over %d runs: %.2f ms\n", runs, avgDuration);
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
    expect(moves.size() == 4) 
      << "Expected 4 legal moves, got " << moves.size() << '\n';  // Only king-side castling and king moves are legal


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
    
    for (auto idx : {0, 1, 2, 3, 4}) {
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

    expect(moves.size() == 1) // Only knight move is legal
      << "Expected 1 legal move, got " << moves.size() << '\n'
      << moves[0].from.toString() << moves[0].to.toString() << '\n'
      << moves[1].from.toString() << moves[1].to.toString() << '\n';
  };

};