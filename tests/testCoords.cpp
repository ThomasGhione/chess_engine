#include "../coords/coords.hpp"
#include "../coords/coords.cpp"

#include <iostream>
#include <cstdint>
#include "ut.hpp"

namespace ut = boost::ut;

ut::suite coordsSuite = [] {
  using namespace ut;

  "invalid parameter"_test = []{
    chess::Coords c = chess::Coords("f10");
  
    std::cout << std::endl << "file:" << c.file << std::endl;

    const uint8_t expectResult = 9;
    expect(c.file == expectResult);
    expect(c.rank == expectResult);
  };
};
