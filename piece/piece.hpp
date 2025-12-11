#ifndef PIECES_HPP
#define PIECES_HPP

#include <cstdint>
#include <vector>
#include <array>

namespace pieces {

using U64 = uint64_t;

static constexpr U64 ONE = 1ULL;

// ==================== UTILS ====================
inline constexpr int8_t fileOf(int8_t sq) noexcept { return static_cast<int8_t>(sq % 8); }
inline constexpr int8_t rankOf(int8_t sq) noexcept { return static_cast<int8_t>(sq / 8); }

constexpr int8_t KNIGHT_OFFSET[8][2] = { {1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2} };
constexpr int8_t KING_OFFSET[8][2] = { {1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1},{0,1} };

// ==================== ATTACK MAPS (color-agnostic salvo pedone) ====================
inline constexpr U64 getPawnAttacks(const int8_t squareIndex, const bool isWhite) noexcept {
	int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);
	U64 attackBitboard = 0ULL;
	// Coords convention: rank 0 = riga 8, rank 7 = riga 1
	// White pawns attack "forward" (rank decreases), Black pawns attack "backward" (rank increases)
	int8_t newRank = rank + (isWhite ? -1 : 1);
	
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

inline constexpr U64 getPawnForwardPushes(int8_t squareIndex, bool isWhite, U64 occupancy) noexcept {
	int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);
	U64 pushBitboard = 0ULL;
	// Coords convention: rank 0 = riga 8, rank 7 = riga 1
	// White pawns move "up" (rank decreases), Black pawns move "down" (rank increases)
	int8_t forwardDir = isWhite ? -1 : 1;
	int8_t oneStepRank = rank + forwardDir;
    
	if (oneStepRank >= 0 && oneStepRank < 8) {
		int8_t oneStepSquare = (oneStepRank * 8) + file;
		if ((occupancy & (ONE << oneStepSquare)) == 0) {
			pushBitboard |= (ONE << oneStepSquare);
			// White pawns start at rank 6 (row 2), Black pawns start at rank 1 (row 7)
			int8_t startRank = isWhite ? 6 : 1;
			if (rank == startRank) {
				int8_t twoStepRank = rank + 2 * forwardDir;
				if (twoStepRank >= 0 && twoStepRank < 8) {
					int8_t twoStepSquare = twoStepRank * 8 + file;
					if ((occupancy & (ONE << twoStepSquare)) == 0)
						pushBitboard |= (ONE << twoStepSquare);
				}
			}
		}
	}
	return pushBitboard;
}

// Returns a bitboard of pawn squares (of color isWhite) that attack the target square
// For a target square, return bitboard of pawn squares (of color isWhite) that would attack the target
inline constexpr U64 getPawnAttackersTo(int8_t targetIndex, bool isWhite) noexcept {
	int8_t tf = fileOf(targetIndex), tr = rankOf(targetIndex);
	U64 attackers = 0ULL;
	// Coords convention: rank 0 = riga 8, rank 7 = riga 1
	// White pawns attack from rank+1 (one rank "lower" numerically), Black pawns attack from rank-1
	int8_t fromRank = tr + (isWhite ? 1 : -1);
	if (fromRank >= 0 && fromRank < 8) {
		if (tf - 1 >= 0) attackers |= ONE << (fromRank * 8 + (tf - 1));
		if (tf + 1 < 8)  attackers |= ONE << (fromRank * 8 + (tf + 1));
	}
	return attackers;
}

inline constexpr U64 getKnightAttacks(int8_t squareIndex) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;

	int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

	U64 attackBitboard = 0ULL;
	
    for (auto &offset : KNIGHT_OFFSET) {
		int8_t newFile = file + offset[0], newRank = rank + offset[1];
		if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
			attackBitboard |= ONE << (newRank * 8 + newFile);
	}
	return attackBitboard;
}

inline constexpr U64 getKingAttacks(int8_t squareIndex) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) return 0ULL;
	
    int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

	U64 attackBitboard = 0ULL;

	for (auto &offset : KING_OFFSET) {
		int8_t newFile = file + offset[0], newRank = rank + offset[1];
		if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
			attackBitboard |= ONE << (newRank * 8 + newFile);
	}

	return attackBitboard;
}

// Sliding (naive, si ferma sul primo blocco)
U64 getBishopAttacks(int8_t squareIndex, U64 occupancy) noexcept;
U64 getRookAttacks(int8_t squareIndex, U64 occupancy) noexcept;
U64 getQueenAttacks(int8_t squareIndex, U64 occupancy) noexcept;

// ==================== UTILS ====================
inline U64 ray(int8_t file, int8_t rank, int8_t deltaFile, int8_t deltaRank, U64 occupancy);


inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_ATTACKS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        table[1][sq] = getPawnAttacks(sq, true);
        table[0][sq] = getPawnAttacks(sq, false);
    }

    return table;
}();

inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_ATTACKERS_TO = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        // 0 = white, 1 = black
        table[0][sq] = getPawnAttackersTo(sq, /*isWhite=*/true);
        table[1][sq] = getPawnAttackersTo(sq, /*isWhite=*/false);
    }

    return table;
}();

inline constexpr std::array<uint64_t, 64> KNIGHT_ATTACKS = []{
    std::array<uint64_t, 64> table{};

    for (int sq = 0; sq < 64; ++sq) {
        table[sq] = getKnightAttacks(sq);
    }

    return table;
}();

inline constexpr std::array<uint64_t, 64> KING_ATTACKS = []{
	std::array<uint64_t, 64> table{};

	for (int sq = 0; sq < 64; ++sq) {
		table[sq] = getKingAttacks(sq);
	}

	return table;
}();


} // namespace pieces

#endif
