#include "./../coords.hpp"

#include <cstdint>
#include "./../../tests/ut.hpp"


namespace ut = boost::ut;

ut::suite coordsConstructorSuite = [] {
  using namespace ut;

  // ** Coords() **
  "Empty constructor"_test = []{
    chess::Coords c = chess::Coords();

    expect(c.file == 255_u8);
    expect(c.rank == 255_u8);
  };

  // ** Coords(uint8_t f, uint8_t r) **
  "Explicit coords, correct parameter"_test = []{
    chess::Coords c = chess::Coords(0, 1);

    expect(c.file == 0_u8);
    expect(c.rank == 1_u8);
  };
  
  "Explicit coords, wrong parameter"_test = []{
    chess::Coords c = chess::Coords(100, 1);

    expect(c.file == 255_u8);
    expect(c.rank == 1_u8);
  };
  
  "Explicit coords, wrong parameter"_test = []{
    chess::Coords c = chess::Coords(100, 30);

    expect(c.file == 255_u8);
    expect(c.rank == 255_u8);
  };
  "Explicit coords, wrong parameter"_test = []{
    chess::Coords c = chess::Coords(4, 31);

    expect(c.file == 4_u8);
    expect(c.rank == 255_u8);
  };

  // ** Coords(const std::string& input) **
  "Invalid parameter, long coord"_test = []{
    // Parametro invalido con stringa piu' lunga
    // di due caratteri
    chess::Coords c = chess::Coords("f10");

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

    expect(c.file == 0_i);
    expect(c.rank == 7_i);
  };

  "String constructor h8"_test = []{
    chess::Coords c = chess::Coords("h8");

    expect(c.file == 7_i);
    expect(c.rank == 7_i);
  };

  "String constructor h1"_test = []{
    chess::Coords c = chess::Coords("h1");

    expect(c.file == 7_i);
    expect(c.rank == 0_i);
  };

  // ** Coords(const Coords& c) **
  "Construct by other object"_test = []{
    chess::Coords c1 = chess::Coords("h1");
    chess::Coords c2 = chess::Coords(c1);

    expect(c2.file == 7_i);
    expect(c2.rank == 0_i);
  };
  
  "Construct by other object"_test = []{
    chess::Coords c1 = chess::Coords("h8");
    chess::Coords c2 = chess::Coords(c1);

    expect(c2.file == 7_i);
    expect(c2.rank == 7_i);
  };
  
  "Construct by other object"_test = []{
    chess::Coords c1 = chess::Coords("a1");
    chess::Coords c2 = chess::Coords(c1);

    expect(c2.file == 0_i);
    expect(c2.rank == 0_i);
  };
}; // suite coordsConstructorSuite
