
//#include <iostream>
//#include <cstdint>
#include "ut.hpp"

namespace ut = boost::ut;

ut::suite piceSuite = [] {
  using namespace ut;

  "Default constructor"_test = []{
  
    expect(true);
  };

};
