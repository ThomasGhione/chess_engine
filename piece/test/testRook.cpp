#include "../../tests/ut.hpp"
#include "../../piece/piece.hpp"
#include "../../coords/coords.hpp"

#include <iostream>

namespace ut = boost::ut;
using namespace ut;

uint64_t coordToIndex3(std::string coord){
  chess::Coords c(coord);
  return c.toIndex();
}

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

  "getRookAttacks corner a1"_test = []{
    uint64_t output = pieces::getRookAttacks(coordToIndex3("a1"), 0ULL); 
    
    uint64_t expect_output = 0;
    expect_output |= (1ULL << coordToIndex3("a2"));
    expect_output |= (1ULL << coordToIndex3("a3"));
    expect_output |= (1ULL << coordToIndex3("a4"));
    expect_output |= (1ULL << coordToIndex3("a5"));
    expect_output |= (1ULL << coordToIndex3("a6"));
    expect_output |= (1ULL << coordToIndex3("a7"));
    expect_output |= (1ULL << coordToIndex3("a8"));
    expect_output |= (1ULL << coordToIndex3("b1"));
    expect_output |= (1ULL << coordToIndex3("c1"));
    expect_output |= (1ULL << coordToIndex3("d1"));
    expect_output |= (1ULL << coordToIndex3("e1"));
    expect_output |= (1ULL << coordToIndex3("f1"));
    expect_output |= (1ULL << coordToIndex3("g1"));
    expect_output |= (1ULL << coordToIndex3("h1"));
    
    expect(expect_output == output);
  };

  "getRookAttacks corner e4"_test = []{
    uint64_t output = pieces::getRookAttacks(coordToIndex3("e4"), 0ULL); 
    
    uint64_t expect_output = 0;
    expect_output |= (1ULL << coordToIndex3("e3"));
    expect_output |= (1ULL << coordToIndex3("e2"));
    expect_output |= (1ULL << coordToIndex3("e1"));
    expect_output |= (1ULL << coordToIndex3("e5"));
    expect_output |= (1ULL << coordToIndex3("e6"));
    expect_output |= (1ULL << coordToIndex3("e7"));
    expect_output |= (1ULL << coordToIndex3("e8"));
    expect_output |= (1ULL << coordToIndex3("d4"));
    expect_output |= (1ULL << coordToIndex3("c4"));
    expect_output |= (1ULL << coordToIndex3("b4"));
    expect_output |= (1ULL << coordToIndex3("a4"));
    expect_output |= (1ULL << coordToIndex3("f4"));
    expect_output |= (1ULL << coordToIndex3("g4"));
    expect_output |= (1ULL << coordToIndex3("h4"));
    
    expect(expect_output == output);
  };

}; // rookSuite
