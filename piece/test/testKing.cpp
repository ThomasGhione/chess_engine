#include "../../tests/ut.hpp"
#include "../../piece/piece.hpp"

#include <iostream>

namespace ut = boost::ut;

uint64_t coordToIndex2(std::string coord){
  return ( '8' - coord.at(1)) *8 + ('h' - coord.at(0));
}

ut::suite kingSuite = [] {
  using namespace ut;
  
  "getKingAttacks corner a1"_test =[]{
    uint64_t output = pieces::getKingAttacks(coordToIndex2("a1")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1 << coordToIndex2("a2"));
    expect_output |= (1 << coordToIndex2("b1"));
    expect_output |= (1 << coordToIndex2("b2"));
    
    expect(expect_output == output);
  };
  
  "getKingAttacks corner h1"_test =[]{
    uint64_t output = pieces::getKingAttacks(coordToIndex2("h1")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1 << coordToIndex2("h2"));
    expect_output |= (1 << coordToIndex2("g1"));
    expect_output |= (1 << coordToIndex2("g2"));
    
    expect(expect_output == output);
  };
}; // kingSuite
