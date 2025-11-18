#include "../../tests/ut.hpp"
#include "../../piece/piece.hpp"
#include "../../coords/coords.hpp"

#include <iostream>

namespace ut = boost::ut;

uint64_t coordToIndex2(std::string coord){
  chess::Coords c(coord);
  return c.toIndex();
}

ut::suite kingSuite = [] {
  using namespace ut;
  
  "getKingAttacks corner a1"_test =[]{
    uint64_t output = pieces::getKingAttacks(coordToIndex2("a1")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1ULL << coordToIndex2("a2"));
    expect_output |= (1ULL << coordToIndex2("b1"));
    expect_output |= (1ULL << coordToIndex2("b2"));
    
    expect(expect_output == output);
  };
  
  "getKingAttacks corner h1"_test =[]{
    uint64_t output = pieces::getKingAttacks(coordToIndex2("h1")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1ULL << coordToIndex2("h2"));
    expect_output |= (1ULL << coordToIndex2("g1"));
    expect_output |= (1ULL << coordToIndex2("g2"));
    
    expect(expect_output == output);
  };

  "getKingAttacks corner a8"_test =[]{
    uint64_t output = pieces::getKingAttacks(coordToIndex2("a8")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1ULL << coordToIndex2("a7"));
    expect_output |= (1ULL << coordToIndex2("b8"));
    expect_output |= (1ULL << coordToIndex2("b7"));
    
    expect(expect_output == output);
  };
  
  "getKingAttacks corner h8"_test =[]{
    uint64_t output = pieces::getKingAttacks(coordToIndex2("h8")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1ULL << coordToIndex2("h7"));
    expect_output |= (1ULL << coordToIndex2("g8"));
    expect_output |= (1ULL << coordToIndex2("g7"));
    
    expect(expect_output == output);
  };

  "getKingAttacks center e4"_test =[]{
    uint64_t output = pieces::getKingAttacks(coordToIndex2("e4")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1ULL << coordToIndex2("d3"));
    expect_output |= (1ULL << coordToIndex2("e3"));
    expect_output |= (1ULL << coordToIndex2("f3"));
    expect_output |= (1ULL << coordToIndex2("d4"));
    expect_output |= (1ULL << coordToIndex2("f4"));
    expect_output |= (1ULL << coordToIndex2("d5"));
    expect_output |= (1ULL << coordToIndex2("e5"));
    expect_output |= (1ULL << coordToIndex2("f5"));
    
    expect(expect_output == output);
  };
}; // kingSuite
