#ifndef PIECES_HPP
#define PIECES_HPP

#include <cstdint>
#include <vector>

namespace pieces {

using U64 = uint64_t;

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
inline U64 ray(int16_t file, int16_t rank, int16_t deltaFile, int16_t deltaRank, U64 occupancy);
inline int16_t fileOf(int16_t sq) noexcept { return static_cast<int16_t>(sq % 8); }
inline int16_t rankOf(int16_t sq) noexcept { return static_cast<int16_t>(sq / 8); }
std::vector<U64> bitboardToIndices(U64 bb) noexcept;

} // namespace pieces

#endif
