#include "../../tests/ut.hpp"
#include "../../piece/piece.hpp"

#include <iostream>

namespace ut = boost::ut;

uint64_t coordToIndex(std::string coord){
  return ( '8' - coord.at(1)) *8 + ('h' - coord.at(0));
}

ut::suite knightSuite = [] {
  using namespace ut;
  
  // ** U64 getKnightAttacks(int16_t squareIndex) **
  "getKnightAttacks negative input"_test = []{
    uint64_t output = pieces::getKnightAttacks(-10); 

    expect(output == 0_u64);
  };
  
  "getKnightAttacks eccessive input"_test = []{
    uint64_t output = pieces::getKnightAttacks(100); 
    expect(output == 0_u64);
  };

  "getKnightAttacks negative input 2"_test = []{
    uint64_t output = pieces::getKnightAttacks(-1); 

    expect(output == 0_u64);
  };
  
  "getKnightAttacks eccessive input 2"_test = []{
    uint64_t output = pieces::getKnightAttacks(64); 
    expect(output == 0_u64);
  };


  "getKnightAttacks corner a1"_test =[]{
    uint64_t output = pieces::getKnightAttacks(coordToIndex("a1")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1 << coordToIndex("b3"));
    expect_output |= (1 << coordToIndex("c2"));
    
    expect(expect_output == output);
  };
  
  "getKnightAttacks corner h1"_test =[]{
    uint64_t output = pieces::getKnightAttacks(coordToIndex("h1")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1 << coordToIndex("g3"));
    expect_output |= (1 << coordToIndex("f2"));
    
    expect(expect_output == output);
  };
  
  "getKnightAttacks corner a8"_test =[]{
    uint64_t output = pieces::getKnightAttacks(coordToIndex("a8")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1 << coordToIndex("b6"));
    expect_output |= (1 << coordToIndex("c7"));
    
    expect(expect_output == output);
  };
  
  "getKnightAttacks corner h8"_test =[]{
    uint64_t output = pieces::getKnightAttacks(coordToIndex("h8")); 
    
    uint64_t expect_output = 0;
    expect_output |= (1 << coordToIndex("g6"));
    expect_output |= (1 << coordToIndex("f7"));
    
    expect(expect_output == output);
  };
}; // knightSuite
