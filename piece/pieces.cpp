#include "pieces.hpp"
#include "../coords/coords.hpp"
#include "../board/board.hpp"


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
// Inizializzatori basici (ritornano il bit del pezzo su squareIndex)
// ------------------------------------------------------------
U64 initPawnBitboard(int16_t squareIndex) noexcept { return (squareIndex >=0 && squareIndex < 64) ? (ONE << squareIndex) : 0ULL; }
U64 initKnightBitboard(int16_t squareIndex) noexcept { return initPawnBitboard(squareIndex); }
U64 initBishopBitboard(int16_t squareIndex) noexcept { return initPawnBitboard(squareIndex); }
U64 initRookBitboard(int16_t squareIndex) noexcept { return initPawnBitboard(squareIndex); }
U64 initQueenBitboard(int16_t squareIndex) noexcept { return initPawnBitboard(squareIndex); }
U64 initKingBitboard(int16_t squareIndex) noexcept { return initPawnBitboard(squareIndex); }

// Placeholder: se il boardBitboard codifica già i pezzi, qui si filtrerebbe.
U64 getPieceBitboard(uint8_t piece, const U64& boardBitboard) noexcept {
	/*
    // filter by piecetype by doing piece | MASK_PIECE:
    U64 pieceMask = static_cast<U64>(piece) | chess::Board::MASK_PIECE;
    return boardBitboard & pieceMask;
    */
    return boardBitboard;
}

// ------------------------------------------------------------
// Knight attacks
// ------------------------------------------------------------
U64 getKnightAttacks(int16_t squareIndex) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;

	int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

	constexpr int16_t OFF[8][2] = {
		{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}
	};

	U64 attackBitboard = 0ULL;
	
    for (auto &offset : OFF) {
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

	for (int16_t deltaFile = -1; deltaFile <= 1; ++deltaFile) {
		for (int16_t deltaRank = -1; deltaRank <= 1; ++deltaRank) {
			if (deltaFile == 0 && deltaRank == 0) continue;
			int16_t newFile = file + deltaFile, newRank = rank + deltaRank;
			if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
				attackBitboard |= ONE << (newRank * 8 + newFile);
		}
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
		if (file - 1 >= 0) attackBitboard |= ONE << (newRank * 8 + (file - 1));
		if (file + 1 < 8)  attackBitboard |= ONE << (newRank * 8 + (file + 1));
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

// ------------------------------------------------------------
// Sliding piece helpers
// ------------------------------------------------------------
static inline U64 ray(int16_t file, int16_t rank, int16_t deltaFile, int16_t deltaRank, U64 occupancy) {
	U64 rayBitboard = 0ULL;
	file += deltaFile; 
    rank += deltaRank;
	
/* 
TODO pls someone test this :(
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC unroll 8
#elif defined(_MSC_VER)
    #pragma loop(unroll)
#endif
*/
	while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
		int16_t square = (rank * 8) + file;
		rayBitboard |= (ONE << square);
		
        if (occupancy & (ONE << square))
            break; // blocked by piece

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



}