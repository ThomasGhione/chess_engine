#include "../piece/piece.cpp"
#include "../piece/piece..hpp"

//#include <iostream>
//#include <cstdint>
#include "ut.hpp"

namespace ut = boost::ut;

ut::suite piceSuite = [] {
  using namespace ut;

  "Default constructor"_test = []{
    chess::Pice p = chess::Piece();
  
    expect(true);
  };

};
