#include "../engine/engine.cpp"
#include "../engine/engine.hpp"

//#include <iostream>
//#include <cstdint>
#include "ut.hpp"

namespace ut = boost::ut;

ut::suite engineSuite = [] {
  using namespace ut;

  "Default constructor"_test = []{
    engine::Engine e = engine::Engine();
  
    expect(true);
  };

};
