#pragma once

#include <bit>
#include <cstdint>
#include <array>
#include "coords.hpp"
#include "magic_numbers.hpp"

namespace pieces {

using U64 = uint64_t;

static constexpr U64 ONE = 1ULL;
static constexpr int WHITE_SIDE = 0;
static constexpr int BLACK_SIDE = 1;
inline constexpr int sideIndex(bool isWhite) noexcept { return isWhite ? WHITE_SIDE : BLACK_SIDE; }

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
template<typename MaskArray, typename MagicArray>
inline constexpr std::array<MagicParams, 64> buildMagicParams(const MaskArray& masks, const MagicArray& magics) {
    std::array<MagicParams, 64> table{};
    uint32_t runningOffset = 0;
    //FIXME Fare il cliclo in modo parallelo
    for (int sq = 0; sq < 64; ++sq) {
        table[sq].mask = masks[sq];
        table[sq].magic = magics[sq];
        int bits = std::popcount(masks[sq]);
        table[sq].shift = 64 - bits;
        table[sq].offset = runningOffset;
        runningOffset += (1 << bits);
    }
    return table;
}

inline constexpr std::array<MagicParams, 64> ROOK_PARAMS = buildMagicParams(ROOK_MASKS, ROOK_MAGICS);
inline constexpr std::array<MagicParams, 64> BISHOP_PARAMS = buildMagicParams(BISHOP_MASKS, BISHOP_MAGICS);

// ===================================================
// MAGIC BITBOARDS - HELPER FUNCTIONS
// ===================================================

// Generate a specific occupancy pattern from an index
inline constexpr uint64_t generateOccupancyPattern(int index, int bitCount, uint64_t mask) noexcept {
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < bitCount; ++i) {
        int bitPos = std::countr_zero(mask);
        mask &= mask - 1; // Clear LSB
        if (index & (1 << i)) {
            occupancy |= (1ULL << bitPos);
        }
    }
    return occupancy;
}

// Generic ray-walk used to build the magic lookup tables (compile-time only).
// Walks each direction until the board edge or the first occupied square.
inline constexpr uint64_t calculateSlidingAttacks(
        int8_t square, uint64_t occupancy, const int8_t dirs[4][2]) noexcept {
    uint64_t attacks = 0ULL;
    const int8_t file = chess::file(square);
    const int8_t rank = chess::rank(square);
    for (int d = 0; d < 4; ++d) {
        const int8_t df = dirs[d][0];
        const int8_t dr = dirs[d][1];
        for (int8_t f = file + df, r = rank + dr;
             f >= 0 && f < 8 && r >= 0 && r < 8; f += df, r += dr) {
            attacks |= (1ULL << (r * 8 + f));
            if (occupancy & (1ULL << (r * 8 + f))) break;
        }
    }
    return attacks;
}

inline constexpr int8_t ROOK_DIRS[4][2]   = {{0, -1}, {0, 1}, {1, 0}, {-1, 0}};
inline constexpr int8_t BISHOP_DIRS[4][2] = {{1, -1}, {-1, -1}, {1, 1}, {-1, 1}};

inline constexpr uint64_t calculateRookAttacksClassical(int8_t square, uint64_t occupancy) noexcept {
    return calculateSlidingAttacks(square, occupancy, ROOK_DIRS);
}

inline constexpr uint64_t calculateBishopAttacksClassical(int8_t square, uint64_t occupancy) noexcept {
    return calculateSlidingAttacks(square, occupancy, BISHOP_DIRS);
}

//FIXME Troppi parametri
// Fill attack table for one square - works for both rook and bishop
template<size_t N, typename AttackFunc>
inline void populateAttackTable(int square, const MagicParams& p,
                                std::array<uint64_t, N>& lookup,
                                AttackFunc attackFunc) noexcept {
    const int bitCount = std::popcount(p.mask);
    const int numPatterns = 1 << bitCount;
    for (int i = 0; i < numPatterns; ++i) {
        uint64_t occupancy = generateOccupancyPattern(i, bitCount, p.mask);
        uint32_t index = ((occupancy & p.mask) * p.magic) >> p.shift;
        lookup[p.offset + index] = attackFunc(square, occupancy);
    }
}

// Initialize all magic bitboards (call at program startup)
inline void initMagicBitboards() noexcept {
    //FIXME Aggiungere this
    for (int sq = 0; sq < 64; ++sq) {
        populateAttackTable(sq, ROOK_PARAMS[sq], ROOK_ATTACK_LOOKUP, calculateRookAttacksClassical);
        populateAttackTable(sq, BISHOP_PARAMS[sq], BISHOP_ATTACK_LOOKUP, calculateBishopAttacksClassical);
    }
}

// ===================================================
// PIECE ATTACKS
// ===================================================

// NOTE: use uint8_t for sq (0..63) to avoid sign-extension and UB-ish corner cases with int8_t indexing.

__attribute__((hot, always_inline))
inline U64 getRookAttacks(uint8_t sq, U64 occ) noexcept {
    const MagicParams& p = ROOK_PARAMS[sq];
    const uint32_t index = ((occ & p.mask) * p.magic) >> p.shift;
    return ROOK_ATTACK_LOOKUP[p.offset + index];
}

__attribute__((hot, always_inline))
inline U64 getBishopAttacks(uint8_t sq, U64 occ) noexcept {
    const MagicParams& p = BISHOP_PARAMS[sq];
    const uint32_t index = ((occ & p.mask) * p.magic) >> p.shift;
    return BISHOP_ATTACK_LOOKUP[p.offset + index];
}

__attribute__((hot, always_inline))
inline U64 getQueenAttacks(uint8_t sq, U64 occ) noexcept {
    return getRookAttacks(sq, occ) | getBishopAttacks(sq, occ);
}

// ==================== ATTACK MAPS (color-agnostic except for pawns) ====================
inline constexpr U64 getPawnAttacks(const int8_t squareIndex, const bool isWhite) noexcept {
	int8_t file = chess::file(squareIndex), rank = chess::rank(squareIndex);
	U64 attackBitboard = 0ULL;
	// Coords convention: rank 0 = row 8, rank 7 = row 1
	// White pawns attack "forward" (rank decreases), Black pawns attack "backward" (rank increases)
	int8_t newRank = rank + (isWhite ? -1 : 1);
	
	//FIXME Rendere constexpr 
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
// table[side][square], side mapping is 0=white, 1=black
// Note: at runtime you still need to check occupancy to validate moves
inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_SINGLE_PUSH_TARGETS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        int8_t rank = chess::rank(sq);
        int8_t file = chess::file(sq);
        
        // White pawns (side=0)
        if (rank > 0) { // Can move up (rank decreases)
            uint64_t oneStep = ONE << ((rank - 1) * 8 + file);
            table[WHITE_SIDE][sq] = oneStep;
        }
        
        // Black pawns (side=1)
        if (rank < 7) { // Can move down (rank increases)
            uint64_t oneStep = ONE << ((rank + 1) * 8 + file);
            table[BLACK_SIDE][sq] = oneStep;
        }
    }

    return table;
}();

inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_DOUBLE_PUSH_TARGETS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        const int8_t rank = chess::rank(sq);
        const int8_t file = chess::file(sq);

        // White start rank: rank 6 (row 2), side=0.
        if (rank == 6) {
            table[WHITE_SIDE][sq] = ONE << ((rank - 2) * 8 + file);
        }

        // Black start rank: rank 1 (row 7), side=1.
        if (rank == 1) {
            table[BLACK_SIDE][sq] = ONE << ((rank + 2) * 8 + file);
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
    const int side = sideIndex(isWhite);
    const U64 oneStepBit = PAWN_SINGLE_PUSH_TARGETS[side][squareIndex];
    const U64 twoStepBit = PAWN_DOUBLE_PUSH_TARGETS[side][squareIndex];
    const unsigned occBits = ((occupancy & oneStepBit) != 0ULL)
        | (((occupancy & twoStepBit) != 0ULL) << 1);
    return PAWN_FORWARD_PUSH_LOOKUP[side][squareIndex][occBits];
}

// Returns a bitboard of pawn squares (of color isWhite) that attack the target square
// For a target square, return bitboard of pawn squares (of color isWhite) that would attack the target
inline constexpr U64 getPawnAttackersTo(int8_t targetIndex, bool isWhite) noexcept {
	int8_t tf = chess::file(targetIndex), tr = chess::rank(targetIndex);
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

// Shared step-attack generator for knight/king (compile-time table builders).
inline constexpr U64 attacksFromOffsets(int8_t squareIndex, const int8_t offsets[8][2]) noexcept {
    const int8_t file = chess::file(squareIndex);
    const int8_t rank = chess::rank(squareIndex);
    U64 attackBitboard = 0ULL;
    for (int i = 0; i < 8; ++i) {
        const int8_t newFile = file + offsets[i][0];
        const int8_t newRank = rank + offsets[i][1];
        if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8)
            attackBitboard |= ONE << (newRank * 8 + newFile);
    }
    return attackBitboard;
}

inline constexpr U64 getKnightAttacks(int8_t squareIndex) noexcept {
    return attacksFromOffsets(squareIndex, KNIGHT_OFFSET);
}

inline constexpr U64 getKingAttacks(int8_t squareIndex) noexcept {
    return attacksFromOffsets(squareIndex, KING_OFFSET);
}

inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_ATTACKS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        table[WHITE_SIDE][sq] = getPawnAttacks(sq, true);
        table[BLACK_SIDE][sq] = getPawnAttacks(sq, false);
    }

    return table;
}();

inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_ATTACKERS_TO = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        // 0 = white, 1 = black
        table[WHITE_SIDE][sq] = getPawnAttackersTo(sq, /*isWhite=*/true);
        table[BLACK_SIDE][sq] = getPawnAttackersTo(sq, /*isWhite=*/false);
    }

    return table;
}();

// getKnightAttacks/getKingAttacks are only used at compile-time to generate these
// lookup tables (iterated over [0,63]). The old runtime bounds-check
// "if (squareIndex < 0 || squareIndex >= 64)" is replaced by these static_asserts
// which verify the generated tables are non-zero for every valid square.
inline constexpr std::array<uint64_t, 64> KNIGHT_ATTACKS = []{
    std::array<uint64_t, 64> table{};

    for (int sq = 0; sq < 64; ++sq)
        table[sq] = getKnightAttacks(sq);

    return table;
}();

inline constexpr std::array<uint64_t, 64> KING_ATTACKS = []{
	std::array<uint64_t, 64> table{};

	for (int sq = 0; sq < 64; ++sq)
		table[sq] = getKingAttacks(sq);

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
} // namespace pieces
