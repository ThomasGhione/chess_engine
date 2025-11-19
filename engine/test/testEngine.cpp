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



  "performance searchPosition depth 4"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 4;

    auto start = std::chrono::high_resolution_clock::now();
    e.search(e.depth);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Attesa che la ricerca venga completata in meno di 5 secondi
    std::cout << "Depth 4 search completed in " << duration << " ms\n";
    std::cout << "Nodes searched: " << engine::Engine::nodesSearched << "\n";
    printf("Depth 4 search completed in %lu ms\n", duration);
    printf("Nodes searched: %lu\n", engine::Engine::nodesSearched);
    expect(duration < 25);
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
    //std::cout << "Depth 6 search completed in " << duration << " ms\n";
    //std::cout << "Nodes searched: " << engine::Engine::nodesSearched <<
    printf("Depth 6 search completed in %lu ms\n", duration);
    printf("Nodes searched: %lu\n", engine::Engine::nodesSearched);
    expect(duration < 2000);
  };

  "avg performance searchPosition depth 6 over 10 runs"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 6;

    constexpr int runs = 10;
    int64_t totalDuration = 0;

    // plays against itself for "runs" moves
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        e.search(e.depth);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        //std::cout << "Run " << (i + 1) << " completed in " << duration << " ms\n";
        printf("Run %d completed in %lu ms\n", i + 1, duration);
        totalDuration += duration;

    }

    double avgDuration = static_cast<double>(totalDuration) / runs;

    // Attesa che la ricerca media venga completata in meno di 20 secondi
    printf("Average Depth 6 search time over %d runs: %.2f ms\n", runs, avgDuration);
    //std::cout << "Average Depth 6 search time over " << runs << " runs: " << avgDuration << " ms\n";
    expect(avgDuration < 2000);
  };

  "calculate 10mln nodes"_test = []{
    engine::Engine e = engine::Engine();
    e.depth = 6;

    constexpr uint64_t targetNodes = 10'000'000;
    e.nodesSearched = 0;

    auto start = std::chrono::high_resolution_clock::now();
    while (e.nodesSearched < targetNodes) {
        e.search(e.depth);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Attesa che la ricerca di 10 milioni di nodi venga completata in meno di 60 secondi
    printf("10 million nodes searched in %lu ms\n", duration);
    expect(duration < 60000);
  };
  */
};
// 16 2573
// 2214 577912