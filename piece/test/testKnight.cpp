#include "../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite knightSuite = [] {
  using namespace ut;

  "Knight"_test = []{
    // In posizione con fen:
    // r3k2r/ppqn1p1p/2p1pnpP/2PpN3/3P4/2NQP3/PP3PP1/R3K2R w KQkq - 2 15

    // Controllare che cavalli possano raggiungere:
    // Da c3: b1 d1 e2 e4 d5 b5 a4
    // Da e5: f3 g4 g6 f7 d7 c6 c4
    // Da d7: b8 b6 c5 e5 f8
    // Da f6: e4 g4 h5 g8
  };
  
  "Knight"_test = []{
    // In posizione con fen:
    // r3k2r/pp1nbpp1/1qp1p2p/3p1b2/2PPnB2/1QN2NP1/PP2PPBP/R3R1K1 w kq - 2 11

    // Controllare che cavalli possano raggiungere:
    // Da c3: b1 d1 e4 d5 b5 a4
    // Da f3: d2 e5 g5 h4
    // Da e4: c3 d2 f2 g3 g5 f6 d6 c5
    // Da d7: b8 c5 e5 f6 f8
  };
}; // knightSuite
