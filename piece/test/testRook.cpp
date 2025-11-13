#include "../ut.hpp"

namespace ut = boost::ut;
using namespace ut;

ut::suite rookSuite = [] {
  "Rook"_test = []{
    // In posizione con fen:
    // 2kr2nr/ppp1qppp/2nb4/5b2/3PP3/2N2N2/PPP3PP/R1BQKB1R w KQ - 1 8

    // Controllare che possa raggiungere queste caselle:
    // b1 g1 d7 e8 f8
  };
  "Rook"_test = []{
    // In posizione con fen:
    // 1nbqkbn1/1pppppp1/r7/p4R1p/P3r2P/R7/1PPPPPP1/1NBQKBN1 w - - 8 7

    // Controllare che possa raggiungere queste caselle:
    // a1 a2 b3 c3 d3 e3 f3 g3 h3
    // a7 a8 b6 c6 d6 e6 f6 g6 h6
    // a4 b4 c4 d4 f4 g4 h4
    // a5 b5 c5 d5 e5 g5 h5
  };
}; // rookSuite
