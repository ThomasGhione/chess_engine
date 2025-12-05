#ifndef PIECES_HPP
#define PIECES_HPP

#include <cstdint>
#include <vector>
#include <array>

namespace pieces {

using U64 = uint64_t;

static constexpr U64 ONE = 1ULL;

// ==================== UTILS ====================
inline constexpr int16_t fileOf(int16_t sq) noexcept { return static_cast<int16_t>(sq % 8); }
inline constexpr int16_t rankOf(int16_t sq) noexcept { return static_cast<int16_t>(sq / 8); }

constexpr int16_t KNIGHT_OFFSET[8][2] = { {1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2} };
constexpr int16_t KING_OFFSET[8][2] = { {1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1},{0,1} };

// ==================== ATTACK MAPS (color-agnostic salvo pedone) ====================
inline constexpr U64 getPawnAttacks(const int16_t squareIndex, const bool isWhite) noexcept {
	int16_t file = fileOf(squareIndex), rank = rankOf(squareIndex);
	U64 attackBitboard = 0ULL;
	int16_t newRank = rank + (isWhite ? 1 : -1);
	
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

inline constexpr U64 getPawnForwardPushes(int16_t squareIndex, bool isWhite, U64 occupancy) noexcept {
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

// Returns a bitboard of pawn squares (of color isWhite) that attack the target square
// For a target square, return bitboard of pawn squares (of color isWhite) that would attack the target
inline constexpr U64 getPawnAttackersTo(int16_t targetIndex, bool isWhite) noexcept {
	int16_t tf = fileOf(targetIndex), tr = rankOf(targetIndex);
	U64 attackers = 0ULL;
	int16_t fromRank = tr - (isWhite ? 1 : -1); // pawns that attack target are located one rank behind target depending on their color
	if (fromRank >= 0 && fromRank < 8) {
		if (tf - 1 >= 0) attackers |= ONE << (fromRank * 8 + (tf - 1));
		if (tf + 1 < 8)  attackers |= ONE << (fromRank * 8 + (tf + 1));
	}
	return attackers;
}

inline constexpr U64 getKnightAttacks(int16_t squareIndex) noexcept {
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

inline constexpr U64 getKingAttacks(int16_t squareIndex) noexcept {
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

// Sliding (naive, si ferma sul primo blocco)
U64 getBishopAttacks(int16_t squareIndex, U64 occupancy) noexcept;
U64 getRookAttacks(int16_t squareIndex, U64 occupancy) noexcept;
U64 getQueenAttacks(int16_t squareIndex, U64 occupancy) noexcept;

// ==================== UTILS ====================
inline U64 ray(int16_t file, int16_t rank, int16_t deltaFile, int16_t deltaRank, U64 occupancy);


inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_ATTACKS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        table[1][sq] = getPawnAttacks(sq, true);
        table[0][sq] = getPawnAttacks(sq, false);
    }

    return table;
}();

inline constexpr std::array<std::array<uint16_t, 64>, 2> PAWN_ATTACKERS_TO = []{
	std::array<std::array<uint16_t, 64>, 2> table{};

	for (int sq = 0; sq < 64; ++sq) {
		table[1][sq] = getPawnAttackersTo(sq, true);
		table[0][sq] = getPawnAttackersTo(sq, false);
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
