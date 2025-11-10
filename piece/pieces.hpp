#ifndef PIECES_HPP
#define PIECES_HPP

#include <cstdint>
#include <vector>

#include "../coords/coords.hpp"
#include "../board/board.hpp"

namespace pieces {

using U64 = uint64_t;

/*
enum Type {
    EMPTY,  // 0000
    PAWN,   // 0001
    KNIGHT, // 0010
    BISHOP, // 0011
    ROOK,   // 0100
    QUEEN,  // 0101
    KING    // 0110
}; */

// ==================== INIT / BITBOARD BUILDERS ====================
U64 getPieceBitboard(chess::Board::piece_id piece, const U64& boardBitboard) noexcept;

U64 initPawnBitboard(int16_t squareIndex) noexcept;
U64 initKnightBitboard(int16_t squareIndex) noexcept;
U64 initBishopBitboard(int16_t squareIndex) noexcept;
U64 initRookBitboard(int16_t squareIndex) noexcept;
U64 initQueenBitboard(int16_t squareIndex) noexcept;
U64 initKingBitboard(int16_t squareIndex) noexcept;

// ==================== ATTACK MAPS (color-agnostic salvo pedone) ====================
U64 getPawnAttacks(int16_t squareIndex, bool isWhite) noexcept;   // solo catture diagonali
U64 getPawnForwardPushes(int16_t squareIndex, bool isWhite, U64 occupancy) noexcept; // avanzamenti (1 o 2 caselle se libere)
U64 getKnightAttacks(int16_t squareIndex) noexcept;
U64 getKingAttacks(int16_t squareIndex) noexcept;

// Sliding (naive, si ferma sul primo blocco)
U64 getBishopAttacks(int16_t squareIndex, U64 occupancy) noexcept;
U64 getRookAttacks(int16_t squareIndex, U64 occupancy) noexcept;
U64 getQueenAttacks(int16_t squareIndex, U64 occupancy) noexcept;

// ==================== UTILS ====================
inline int16_t fileOf(int16_t sq) noexcept { return static_cast<int16_t>(sq % 8); }
inline int16_t rankOf(int16_t sq) noexcept { return static_cast<int16_t>(sq / 8); }
std::vector<U64> bitboardToIndices(U64 bb) noexcept;

} // namespace pieces
#endif