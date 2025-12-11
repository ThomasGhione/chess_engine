#include "piece.hpp"

// Se servono ottimizzazioni future: sostituire con magic bitboards.
namespace pieces {


// ------------------------------------------------------------
// Sliding piece helpers
// ------------------------------------------------------------
inline U64 ray(int8_t file, int8_t rank, int8_t deltaFile, int8_t deltaRank, U64 occupancy) {
	U64 rayBitboard = 0ULL;
	file += deltaFile;
	rank += deltaRank;

	while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
		const int8_t square = static_cast<int8_t>(rank * 8 + file);
		const U64 mask = (ONE << square);
		rayBitboard |= mask;
		if (occupancy & mask) {
			break; // include the blocker square, then stop
		}
		file += deltaFile;
		rank += deltaRank;
	}
	return rayBitboard;
}

U64 getBishopAttacks(int8_t squareIndex, U64 occupancy) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;
	
    int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

    return ray(file, rank, 1, 1, occupancy) |
           ray(file, rank, -1, 1, occupancy) |
           ray(file, rank, 1, -1, occupancy) |
           ray(file, rank, -1, -1, occupancy);
}

U64 getRookAttacks(int8_t squareIndex, U64 occupancy) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;

    int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

    return ray(file, rank, 1, 0, occupancy) |
           ray(file, rank, -1, 0, occupancy) |
           ray(file, rank, 0, 1, occupancy) |
           ray(file, rank, 0, -1, occupancy);
}

U64 getQueenAttacks(int8_t squareIndex, U64 occupancy) noexcept {
	return getBishopAttacks(squareIndex, occupancy) | 
           getRookAttacks(squareIndex, occupancy);
}
} // namespace pieces
