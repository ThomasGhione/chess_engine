// At the moment it appears to be done.

#include "../coords/coords.hpp"

#include <cstdint>
#include "ut.hpp"

namespace ut = boost::ut;

ut::suite coordsSuite = [] {
  using namespace ut;

  "Invalid parameter, long coord"_test = []{
    // Parametro invalido con stringa piu' lunga
    // di due caratteri
    chess::Coords c = chess::Coords("f10");

    expect(c.file == 255_u8);
    expect(c.rank == 255_u8);
  };

  "Empty constructor"_test = []{
    chess::Coords c = chess::Coords();

    expect(c.file == 255_u8);
    expect(c.rank == 255_u8);
  };


  "String constructor a1"_test = []{
    
    chess::Coords c = chess::Coords("a1");

    expect(c.file == 0_i);
    expect(c.rank == 0_i);
  };

  "String constructor a8"_test = []{
    
    chess::Coords c = chess::Coords("a8");

    expect(c.file == 7_i);
    expect(c.rank == 0_i);
  };

  "String constructor h8"_test = []{
    
    chess::Coords c = chess::Coords("h8");

    expect(c.file == 7_i);
    expect(c.rank == 7_i);
  };

  "String constructor h1"_test = []{
    
    chess::Coords c = chess::Coords("h1");

    expect(c.file == 0_i);
    expect(c.rank == 7_i);
  };
  
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
