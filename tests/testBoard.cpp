#include "../board/board.cpp"
#include "../board/board.hpp"

//#include <iostream>
//#include <cstdint>
#include "ut.hpp"

namespace ut = boost::ut;

ut::suite boardSuite = [] {
  using namespace ut;

  "Default constructor"_test = []{
    chess::Board b = chess::Board();
  
    expect(true);
  };

};
