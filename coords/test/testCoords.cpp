// At the moment it appears to be done.

#include "../coords.hpp"

#include <cstdint>
#include "./../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite coordsSuite = [] {
  using namespace ut;

  "Equal operator"_test = []{
    
    chess::Coords c1 = chess::Coords("h1");
    chess::Coords c2 = chess::Coords("h1");

    expect( c1 == c2);
  };


  "Not-Equal operator, different file"_test = []{
    
    chess::Coords c1 = chess::Coords("h1");
    chess::Coords c2 = chess::Coords("h2");

    expect( c1 != c2);
  };
  
  "Not-Equal operator, different rank"_test = []{
    
    chess::Coords c1 = chess::Coords("h1");
    chess::Coords c2 = chess::Coords("g1");

    expect( c1 != c2);
  };
  
  "Update coord"_test = []{
    
    chess::Coords c1 = chess::Coords("h1");
    chess::Coords c2 = chess::Coords("g1");
    
    c1.update(c2);
    expect( c1 == c2);
  };

  
  "= operator"_test = []{
    
    chess::Coords c1 = chess::Coords("h1");
    chess::Coords c2 = chess::Coords("g1");
    
    c1 = c2;
    expect( c1 == c2);
  };
};
