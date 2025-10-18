#ifndef KNIGHT_HPP
#define KNIGHT_HPP

#include <array>
#include "../coords/coords.hpp"

namespace chess {
    
class Knight{
public:
  //Knight(Coords c, piece_id i, bool color);
  Knight(Coords c, bool color);

  void getAllLegalMoves(const std::array<chess::Piece, 64>& board);
    
private:
  static constexpr int directions[8][2] = {
      {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
      {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
  };
};

}

#endif
