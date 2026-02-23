#ifndef PIECES_HPP
#define PIECES_HPP

#include <cstdint>
#include <vector>
#include <array>
#include "magic_numbers.hpp"

namespace pieces {

using U64 = uint64_t;

static constexpr U64 ONE = 1ULL;

// ==================== UTILS ====================
inline constexpr int8_t fileOf(int8_t sq) noexcept { return static_cast<int8_t>(sq % 8); }
inline constexpr int8_t rankOf(int8_t sq) noexcept { return static_cast<int8_t>(sq / 8); }

inline constexpr int8_t KNIGHT_OFFSET[8][2] = { {1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2} };
inline constexpr int8_t KING_OFFSET[8][2] = { {1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1},{0,1} };

// ===================================================
// MAGIC BITBOARDS
// ===================================================

// Lookup table sizes
constexpr size_t ROOK_LOOKUP_SIZE = 102400;    // ~800 KB
constexpr size_t BISHOP_LOOKUP_SIZE = 5248;    // ~40 KB

// Attack lookup tables (runtime storage)
inline std::array<uint64_t, ROOK_LOOKUP_SIZE> ROOK_ATTACK_LOOKUP;
inline std::array<uint64_t, BISHOP_LOOKUP_SIZE> BISHOP_ATTACK_LOOKUP;

// ===================================================
// MAGIC BITBOARDS
// ===================================================

struct MagicParams {
    uint64_t mask;
    uint64_t magic;
    uint32_t shift;
    uint32_t offset;
};

// Pre-computed params (constexpr, .rodata)
inline constexpr std::array<MagicParams, 64> ROOK_PARAMS = []{
    std::array<MagicParams, 64> table{};
    uint32_t runningOffset = 0;
    for (int sq = 0; sq < 64; ++sq) {
        table[sq].mask = ROOK_MASKS[sq];
        table[sq].magic = ROOK_MAGICS[sq];
        int bits = __builtin_popcountll(ROOK_MASKS[sq]);
        table[sq].shift = 64 - bits;
        table[sq].offset = runningOffset;
        runningOffset += (1 << bits);
    }
    return table;
}();

inline constexpr std::array<MagicParams, 64> BISHOP_PARAMS = []{
    std::array<MagicParams, 64> table{};
    uint32_t runningOffset = 0;
    for (int sq = 0; sq < 64; ++sq) {
        table[sq].mask = BISHOP_MASKS[sq];
        table[sq].magic = BISHOP_MAGICS[sq];
        int bits = __builtin_popcountll(BISHOP_MASKS[sq]);
        table[sq].shift = 64 - bits;
        table[sq].offset = runningOffset;
        runningOffset += (1 << bits);
    }
    return table;
}();

// ===================================================
// MAGIC BITBOARDS - HELPER FUNCTIONS
// ===================================================

// Generate a specific occupancy pattern from an index
inline constexpr uint64_t generateOccupancyPattern(int index, int bitCount, uint64_t mask) noexcept {
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < bitCount; ++i) {
        int bitPos = __builtin_ctzll(mask);
        mask &= mask - 1; // Clear LSB
        if (index & (1 << i)) {
            occupancy |= (1ULL << bitPos);
        }
    }
    return occupancy;
}

// Compute rook attacks in the classic way (ground truth)
inline constexpr uint64_t calculateRookAttacksClassical(int8_t square, uint64_t occupancy) noexcept {
    uint64_t attacks = 0ULL;
    int8_t file = fileOf(square);
    int8_t rank = rankOf(square);

    // North (rank decreases)
    for (int8_t r = rank - 1; r >= 0; --r) {
        attacks |= (1ULL << (r * 8 + file));
        if (occupancy & (1ULL << (r * 8 + file))) break;
    }
    // South (rank increases)
    for (int8_t r = rank + 1; r < 8; ++r) {
        attacks |= (1ULL << (r * 8 + file));
        if (occupancy & (1ULL << (r * 8 + file))) break;
    }
    // East (file increases)
    for (int8_t f = file + 1; f < 8; ++f) {
        attacks |= (1ULL << (rank * 8 + f));
        if (occupancy & (1ULL << (rank * 8 + f))) break;
    }
    // West (file decreases)
    for (int8_t f = file - 1; f >= 0; --f) {
        attacks |= (1ULL << (rank * 8 + f));
        if (occupancy & (1ULL << (rank * 8 + f))) break;
    }

    return attacks;
}

// Compute bishop attacks in the classic way (ground truth)
inline constexpr uint64_t calculateBishopAttacksClassical(int8_t square, uint64_t occupancy) noexcept {
    uint64_t attacks = 0ULL;
    int8_t file = fileOf(square);
    int8_t rank = rankOf(square);

    // NE (file increases, rank decreases)
    for (int8_t f = file + 1, r = rank - 1; f < 8 && r >= 0; ++f, --r) {
        attacks |= (1ULL << (r * 8 + f));
        if (occupancy & (1ULL << (r * 8 + f))) break;
    }
    // NW (file decreases, rank decreases)
    for (int8_t f = file - 1, r = rank - 1; f >= 0 && r >= 0; --f, --r) {
        attacks |= (1ULL << (r * 8 + f));
        if (occupancy & (1ULL << (r * 8 + f))) break;
    }
    // SE (file increases, rank increases)
    for (int8_t f = file + 1, r = rank + 1; f < 8 && r < 8; ++f, ++r) {
        attacks |= (1ULL << (r * 8 + f));
        if (occupancy & (1ULL << (r * 8 + f))) break;
    }
    // SW (file decreases, rank increases)
    for (int8_t f = file - 1, r = rank + 1; f >= 0 && r < 8; --f, ++r) {
        attacks |= (1ULL << (r * 8 + f));
        if (occupancy & (1ULL << (r * 8 + f))) break;
    }

    return attacks;
}

// Fill attack table for one square (Rook) - optimized version
inline void populateRookAttackTable(int square) noexcept {
    const MagicParams& p = ROOK_PARAMS[square];
    const int bitCount = __builtin_popcountll(p.mask);
    const int numPatterns = 1 << bitCount;

    for (int i = 0; i < numPatterns; ++i) {
        uint64_t occupancy = generateOccupancyPattern(i, bitCount, p.mask);
        uint32_t index = static_cast<uint32_t>(
            ((occupancy & p.mask) * p.magic) >> p.shift
        );
        uint64_t attacks = calculateRookAttacksClassical(static_cast<int8_t>(square), occupancy);
        ROOK_ATTACK_LOOKUP[p.offset + index] = attacks;
    }
}

// Fill attack table for one square (Bishop) - optimized version
inline void populateBishopAttackTable(int square) noexcept {
    const MagicParams& p = BISHOP_PARAMS[square];
    const int bitCount = __builtin_popcountll(p.mask);
    const int numPatterns = 1 << bitCount;

    for (int i = 0; i < numPatterns; ++i) {
        uint64_t occupancy = generateOccupancyPattern(i, bitCount, p.mask);
        uint32_t index = static_cast<uint32_t>(
            ((occupancy & p.mask) * p.magic) >> p.shift
        );
        uint64_t attacks = calculateBishopAttacksClassical(static_cast<int8_t>(square), occupancy);
        BISHOP_ATTACK_LOOKUP[p.offset + index] = attacks;
    }
}

// Initialize all magic bitboards (call at program startup)
inline void initMagicBitboards() noexcept {
    for (int sq = 0; sq < 64; ++sq) {
        populateRookAttackTable(sq);
        populateBishopAttackTable(sq);
    }
}

// ===================================================
// PIECE ATTACKS
// ===================================================

// NOTE: use uint8_t for sq (0..63) to avoid sign-extension and UB-ish corner cases with int8_t indexing.

__attribute__((hot, always_inline))
inline U64 getRookAttacks(uint8_t sq, U64 occ) noexcept {
    const MagicParams& p = ROOK_PARAMS[sq];
    const uint32_t index = static_cast<uint32_t>(((occ & p.mask) * p.magic) >> p.shift);
    return ROOK_ATTACK_LOOKUP[p.offset + index];
}

__attribute__((hot, always_inline))
inline U64 getBishopAttacks(uint8_t sq, U64 occ) noexcept {
    const MagicParams& p = BISHOP_PARAMS[sq];
    const uint32_t index = static_cast<uint32_t>(((occ & p.mask) * p.magic) >> p.shift);
    return BISHOP_ATTACK_LOOKUP[p.offset + index];
}

__attribute__((hot, always_inline))
inline U64 getQueenAttacks(uint8_t sq, U64 occ) noexcept {
    return getRookAttacks(sq, occ) | getBishopAttacks(sq, occ);
}

// ==================== ATTACK MAPS (color-agnostic except for pawns) ====================
inline constexpr U64 getPawnAttacks(const int8_t squareIndex, const bool isWhite) noexcept {
	int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);
	U64 attackBitboard = 0ULL;
	// Coords convention: rank 0 = row 8, rank 7 = row 1
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


// Lookup table for pawn push target squares (without occupancy checks)
// table[isWhite][square] = bitboard with 1-step and 2-step target squares
// Note: at runtime you still need to check occupancy to validate moves
inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_SINGLE_PUSH_TARGETS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        int8_t rank = rankOf(sq);
        int8_t file = fileOf(sq);
        
        // White pawns (isWhite=1, index 1)
        if (rank > 0) { // Can move up (rank decreases)
            uint64_t oneStep = ONE << ((rank - 1) * 8 + file);
            table[1][sq] = oneStep;
        }
        
        // Black pawns (isWhite=0, index 0)
        if (rank < 7) { // Can move down (rank increases)
            uint64_t oneStep = ONE << ((rank + 1) * 8 + file);
            table[0][sq] = oneStep;
        }
    }

    return table;
}();

inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_DOUBLE_PUSH_TARGETS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        const int8_t rank = rankOf(sq);
        const int8_t file = fileOf(sq);

        // White start rank: rank 6 (row 2).
        if (rank == 6) {
            table[1][sq] = ONE << ((rank - 2) * 8 + file);
        }

        // Black start rank: rank 1 (row 7).
        if (rank == 1) {
            table[0][sq] = ONE << ((rank + 2) * 8 + file);
        }
    }

    return table;
}();

inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_PUSH_TARGETS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};
    for (int c = 0; c < 2; ++c) {
        for (int sq = 0; sq < 64; ++sq) {
            table[c][sq] = PAWN_SINGLE_PUSH_TARGETS[c][sq] | PAWN_DOUBLE_PUSH_TARGETS[c][sq];
        }
    }
    return table;
}();

// Occupancy bits relevant to pawn forward pushes:
// bit0 = one-step destination occupied
// bit1 = two-step destination occupied
inline constexpr std::array<std::array<std::array<uint64_t, 4>, 64>, 2> PAWN_FORWARD_PUSH_LOOKUP = []{
    std::array<std::array<std::array<uint64_t, 4>, 64>, 2> table{};

    for (int c = 0; c < 2; ++c) {
        for (int sq = 0; sq < 64; ++sq) {
            const uint64_t oneStep = PAWN_SINGLE_PUSH_TARGETS[c][sq];
            const uint64_t twoStep = PAWN_DOUBLE_PUSH_TARGETS[c][sq];
            for (int occBits = 0; occBits < 4; ++occBits) {
                uint64_t result = 0ULL;
                const bool oneBlocked = (occBits & 0x1) != 0;
                const bool twoBlocked = (occBits & 0x2) != 0;

                if (oneStep && !oneBlocked) {
                    result |= oneStep;
                    if (twoStep && !twoBlocked) {
                        result |= twoStep;
                    }
                }

                table[c][sq][occBits] = result;
            }
        }
    }

    return table;
}();

__attribute__((hot, always_inline))
inline constexpr U64 getPawnForwardPushes(uint8_t squareIndex, bool isWhite, U64 occupancy) noexcept {
    const int colorIndex = static_cast<int>(isWhite);
    const U64 oneStepBit = PAWN_SINGLE_PUSH_TARGETS[colorIndex][squareIndex];
    const U64 twoStepBit = PAWN_DOUBLE_PUSH_TARGETS[colorIndex][squareIndex];
    const unsigned occBits = static_cast<unsigned>((occupancy & oneStepBit) != 0ULL)
        | (static_cast<unsigned>((occupancy & twoStepBit) != 0ULL) << 1);
    return PAWN_FORWARD_PUSH_LOOKUP[colorIndex][squareIndex][occBits];
}

// Returns a bitboard of pawn squares (of color isWhite) that attack the target square
// For a target square, return bitboard of pawn squares (of color isWhite) that would attack the target
inline constexpr U64 getPawnAttackersTo(int8_t targetIndex, bool isWhite) noexcept {
	int8_t tf = fileOf(targetIndex), tr = rankOf(targetIndex);
	U64 attackers = 0ULL;
	// Coords convention: rank 0 = row 8, rank 7 = row 1
	// White pawns attack from rank+1 (one rank "lower" numerically), Black pawns attack from rank-1
	int8_t fromRank = tr + (isWhite ? 1 : -1);
	if (fromRank >= 0 && fromRank < 8) {
		if (tf - 1 >= 0) attackers |= ONE << (fromRank * 8 + (tf - 1));
		if (tf + 1 < 8)  attackers |= ONE << (fromRank * 8 + (tf + 1));
	}
	return attackers;
}

inline constexpr U64 getKnightAttacks(int8_t squareIndex) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) [[unlikely]] return 0ULL;

	int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

	U64 attackBitboard = 0ULL;
	
    for (const auto &offset : KNIGHT_OFFSET) {
		int8_t newFile = file + offset[0], newRank = rank + offset[1];
		if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
			attackBitboard |= ONE << (newRank * 8 + newFile);
	}
	return attackBitboard;
}

inline constexpr U64 getKingAttacks(int8_t squareIndex) noexcept {
	if (squareIndex < 0 || squareIndex >= 64) [[unlikely]] return 0ULL;
	
    int8_t file = fileOf(squareIndex), rank = rankOf(squareIndex);

	U64 attackBitboard = 0ULL;

	for (const auto &offset : KING_OFFSET) {
		int8_t newFile = file + offset[0], newRank = rank + offset[1];
		if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
			attackBitboard |= ONE << (newRank * 8 + newFile);
	}

	return attackBitboard;
}




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


// ==================== PIECE MOVE DISPATCH TABLE ====================
template<uint8_t PieceType>
[[nodiscard]] __attribute__((hot, always_inline))
inline constexpr uint64_t generateMovesByType(uint8_t index, uint64_t occupancy) noexcept {
    if constexpr (PieceType == 0x2) return KNIGHT_ATTACKS[index];              // KNIGHT
    if constexpr (PieceType == 0x3) return getBishopAttacks(index, occupancy); // BISHOP
    if constexpr (PieceType == 0x4) return getRookAttacks(index, occupancy);   // ROOK
    if constexpr (PieceType == 0x5) return getQueenAttacks(index, occupancy);  // QUEEN
    if constexpr (PieceType == 0x6) return KING_ATTACKS[index];                // KING
    return 0ULL;
}

[[nodiscard]] __attribute__((hot, always_inline))
inline uint64_t dispatchPieceMoves(uint8_t pieceType, uint8_t index, uint64_t occupancy) noexcept {
    switch (pieceType) {
        case 0x2: return KNIGHT_ATTACKS[index];              // KNIGHT
        case 0x3: return getBishopAttacks(index, occupancy); // BISHOP
        case 0x4: return getRookAttacks(index, occupancy);   // ROOK
        case 0x5: return getQueenAttacks(index, occupancy);  // QUEEN
        case 0x6: return KING_ATTACKS[index];                // KING
        default:  return 0ULL;
    }
}


} // namespace pieces

#endif
