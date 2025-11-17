#include "../../piece/piece.hpp"
#include "../../tests/ut.hpp"

namespace ut = boost::ut;
using namespace ut;

void testerRange(int16_t start, int16_t end, int16_t expect_data){
  for(int16_t data = start; data < end; data++){
    int16_t output = pieces::rankOf(data);
    expect(output == expect_data);
  }
}

ut::suite helperSuite = [] {
  // ** int16_t fileOf(int16_t sq) **
  "rank in chessboard"_test = []{
    for(int16_t i = 0; i < 8; i++){
      testerRange(8*i, 8*(i+1), i);
    }
  };

  // ** int16_t fileOf(int16_t sq) **
  "file in chessboard"_test = []{
    for(int16_t i = 0; i < 8; i++){
      for(int16_t j = 0; j < 8; j++){
        
        int16_t output = pieces::fileOf(8*i+j);
        expect(output == j);
      }
    }
  };
  // ** std::vector<U64> bitboardToIndices(U64 bb) **
  "index of bit-board"_test =[]{
    uint64_t controlled_bit = 1;
    for(int i = 0; i < 64; i++){
      auto v = pieces::bitboardToIndices(controlled_bit);
      
      auto it = find(v.begin(), v.end(), i);
      
      expect(it != v.end());

      // Fai uno shift di un bit
      controlled_bit = controlled_bit << 1;
    }
  };


}; // helperSuite


