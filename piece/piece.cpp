#include "piece.hpp"

// Se servono ottimizzazioni future: sostituire con magic bitboards.
namespace pieces {

static constexpr U64 ONE = 1ULL; // 0x0000000000000001

// ------------------------------------------------------------
// Utilità: estrai indici dai bit a 1
// ------------------------------------------------------------
std::vector<U64> bitboardToIndices(U64 bitboard) noexcept {
	std::vector<U64> indices;
	indices.reserve(16); // heuristic
	while (bitboard) {
#if defined(_MSC_VER)
		unsigned long index;
		_BitScanForward64(&index, bitboard);
		indices.push_back(static_cast<U64>(index));
		bitboard &= (bitboard - 1);
#else
		U64 index = __builtin_ctzll(bitboard);
		indices.push_back(index);
		bitboard &= (bitboard - 1);
#endif
	}
	return indices;
}


// ------------------------------------------------------------
// Knight attacks
// ------------------------------------------------------------
U64 getKnightAttacks(int16_t squareIndex) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;

	int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

	U64 attackBitboard = 0ULL;
	
    for (auto &offset : KNIGHT_OFFSET) {
		int16_t newFile = file + offset[0], newRank = rank + offset[1];
		if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
			attackBitboard |= ONE << (newRank * 8 + newFile);
	}
	return attackBitboard;
}

// ------------------------------------------------------------
// King attacks
// ------------------------------------------------------------
U64 getKingAttacks(int16_t squareIndex) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;
	
    int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

	U64 attackBitboard = 0ULL;

	for (auto &offset : KING_OFFSET) {
		int16_t newFile = file + offset[0], newRank = rank + offset[1];
		if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
			attackBitboard |= ONE << (newRank * 8 + newFile);
	}

	return attackBitboard;
}


// ------------------------------------------------------------
// Pawn capture (diagonali) + forward pushes
// ------------------------------------------------------------
U64 getPawnAttacks(int16_t squareIndex, bool isWhite) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;

	int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);
	U64 attackBitboard = 0ULL;
	int16_t forwardDir = isWhite ? 1 : -1;
	int16_t newRank = rank + forwardDir;
	
	if (newRank >= 0 && newRank < 8) {
		if (file - 1 >= 0) {
			attackBitboard |= ONE << (newRank * 8 + (file - 1));
		}
		if (file + 1 < 8) {
			attackBitboard |= ONE << (newRank * 8 + (file + 1));
		}
	}
	return attackBitboard;
}

U64 getPawnForwardPushes(int16_t squareIndex, bool isWhite, U64 occupancy) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;
    
	int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);
	U64 pushBitboard = 0ULL;
	int16_t forwardDir = isWhite ? 1 : -1;
	int16_t oneStepRank = rank + forwardDir;
    
	if (oneStepRank >= 0 && oneStepRank < 8) {
		int16_t oneStepSquare = (oneStepRank * 8) + file;
		if ((occupancy & (ONE << oneStepSquare)) == 0) {
			pushBitboard |= (ONE << oneStepSquare);
			int16_t startRank = isWhite ? 1 : 6;
			if (rank == startRank) {
				int16_t twoStepRank = rank + 2 * forwardDir;
				if (twoStepRank >= 0 && twoStepRank < 8) {
					int16_t twoStepSquare = twoStepRank * 8 + file;
					if ((occupancy & (ONE << twoStepSquare)) == 0)
						pushBitboard |= (ONE << twoStepSquare);
				}
			}
		}
	}
	return pushBitboard;
}

// For a target square, return bitboard of pawn squares (of color isWhite) that would attack the target
U64 getPawnAttackersTo(int16_t targetIndex, bool isWhite) noexcept {
	if (targetIndex < 0 || targetIndex >= 64) return 0ULL;
	int16_t tf = fileOf(targetIndex), tr = rankOf(targetIndex);
	U64 attackers = 0ULL;
	int16_t fromRank = tr - (isWhite ? 1 : -1); // pawns that attack target are located one rank behind target depending on their color
	if (fromRank >= 0 && fromRank < 8) {
		if (tf - 1 >= 0) attackers |= ONE << (fromRank * 8 + (tf - 1));
		if (tf + 1 < 8)  attackers |= ONE << (fromRank * 8 + (tf + 1));
	}
	return attackers;
}

// ------------------------------------------------------------
// Sliding piece helpers
// ------------------------------------------------------------
inline U64 ray(int16_t file, int16_t rank, int16_t deltaFile, int16_t deltaRank, U64 occupancy) {
	U64 rayBitboard = 0ULL;
	file += deltaFile;
	rank += deltaRank;

	while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
		const int16_t square = static_cast<int16_t>(rank * 8 + file);
		const U64 mask = (ONE << (rank * 8 + file));
		rayBitboard |= mask;
		if (occupancy & mask) {
			break; // include the blocker square, then stop
		}
		file += deltaFile;
		rank += deltaRank;
	}
	return rayBitboard;
}

U64 getBishopAttacks(int16_t squareIndex, U64 occupancy) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;
	
    int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

    return ray(file, rank, 1, 1, occupancy) |
           ray(file, rank, -1, 1, occupancy) |
           ray(file, rank, 1, -1, occupancy) |
           ray(file, rank, -1, -1, occupancy);
}

U64 getRookAttacks(int16_t squareIndex, U64 occupancy) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;

    int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

    return ray(file, rank, 1, 0, occupancy) |
           ray(file, rank, -1, 0, occupancy) |
           ray(file, rank, 0, 1, occupancy) |
           ray(file, rank, 0, -1, occupancy);
}

U64 getQueenAttacks(int16_t squareIndex, U64 occupancy) noexcept {
	return getBishopAttacks(squareIndex, occupancy) | 
           getRookAttacks(squareIndex, occupancy);
}
} // namespace pieces
