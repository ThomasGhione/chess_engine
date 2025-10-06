#include "knight.hpp"

namespace chess {

Knight::Knight(struct coords c, piece_id i, bool color)
    : Piece(c, P_KNIGHT, color)
{}

bool Knight::updateLegalMoves() {
    return true; // placeholder
}

}