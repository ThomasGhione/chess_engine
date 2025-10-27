#include "../engine/engine.hpp"

//#include <iostream>
//#include <cstdint>
#include "ut.hpp"

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
  }

  "isMate"_test = []{
    // Creare board con fen:
    // r3k2r/pp1nQppp/5n2/3p4/1B6/P4N2/1Pq2PPP/RN2K2R b KQkq - 0 12
    // Attesa isMate: true
  }
  
  "isMate"_test = []{
    // Creare board con fen:
    // r1bk2nr/pppp1ppp/2n5/1N6/4q3/2P5/P1P1BPPP/R1BQ1RK1 b - - 2 9
    // Attesa isMate: false
  }
};
