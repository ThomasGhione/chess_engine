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

enum Direction : uint8_t {
    N=0, S, E, W,
    NE, NW, SE, SW,
    DIRECTION_COUNT
};

// ===================================================
// RAY MASK PRECALCULATION
// ===================================================
constexpr U64 generateRayMask(int8_t square, Direction dir) {
    int8_t file = fileOf(square);
    int8_t rank = rankOf(square);
    int8_t df = 0, dr = 0;

    switch(dir) {
        case N:  df=0; dr=1; break;
        case S:  df=0; dr=-1; break;
        case E:  df=1; dr=0; break;
        case W:  df=-1; dr=0; break;
        case NE: df=1; dr=1; break;
        case NW: df=-1; dr=1; break;
        case SE: df=1; dr=-1; break;
        case SW: df=-1; dr=-1; break;
        default: break;
    }

    U64 mask = 0ULL;
    file += df; rank += dr;
    while(file>=0 && file<8 && rank>=0 && rank<8) {
        mask |= (ONE << (rank*8 + file));
        file += df; rank += dr;
    }
    return mask;
}

inline constexpr auto generateAllRayMasks() {
    std::array<std::array<U64, DIRECTION_COUNT>, 64> table{};
    for(int sq=0;sq<64;sq++)
        for(int dir=0;dir<DIRECTION_COUNT;dir++)
            table[sq][dir] = generateRayMask(sq, static_cast<Direction>(dir));
    return table;
}

inline constexpr auto RAY_MASK = generateAllRayMasks();

// ===================================================
// BRANCHLESS O(1) RAY
// ===================================================
inline constexpr U64 ray(int8_t square, Direction dir, U64 occupancy) noexcept {
    U64 mask = RAY_MASK[square][dir];
    U64 blockers = mask & occupancy;
    if (!blockers) return mask;

    // trova il blocker più vicino in base alla direzione
    int8_t blockerSq;
    switch (dir) {
        // Per direzioni verso l'alto/destra: primo blocker (LSB)
        case N: case NE: case NW: case E:
            blockerSq = __builtin_ctzll(blockers);
            // Include blocker, rimuovi tutto oltre
            return mask & ((1ULL << (blockerSq + 1)) - 1ULL);
        
        // Per direzioni verso il basso/sinistra: ultimo blocker (MSB)
        case S: case SE: case SW: case W:
            blockerSq = 63 - __builtin_clzll(blockers);
            // Include blocker, rimuovi tutto prima
            return mask & ~((1ULL << blockerSq) - 1ULL);
        
        default: 
            return 0ULL;
    }
}

// ===================================================
// PIECE ATTACKS
// ===================================================
inline constexpr U64 getBishopAttacks(int8_t sq, U64 occ) noexcept {
    return ray(sq, NE, occ) | ray(sq, NW, occ) | ray(sq, SE, occ) | ray(sq, SW, occ);
}

inline constexpr U64 getRookAttacks(int8_t sq, U64 occ) noexcept {
    return ray(sq, N, occ) | ray(sq, S, occ) | ray(sq, E, occ) | ray(sq, W, occ);
}

inline constexpr U64 getQueenAttacks(int8_t sq, U64 occ) noexcept {
    return getBishopAttacks(sq, occ) | getRookAttacks(sq, occ);
}


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


// Lookup table per target squares dei pawn pushes (senza considerare occupancy)
// table[isWhite][square] = bitboard con 1-step e 2-step target squares
// Nota: a runtime devi ancora controllare occupancy per validare le mosse
inline constexpr std::array<std::array<uint64_t, 64>, 2> PAWN_PUSH_TARGETS = []{
    std::array<std::array<uint64_t, 64>, 2> table{};

    for (int sq = 0; sq < 64; ++sq) {
        int8_t rank = rankOf(sq);
        int8_t file = fileOf(sq);
        
        // White pawns (isWhite=1, index 1)
        if (rank > 0) { // Can move up (rank decreases)
            uint64_t oneStep = ONE << ((rank - 1) * 8 + file);
            table[1][sq] = oneStep;
            
            // Two-step from starting rank (rank 6 = row 2)
            if (rank == 6 && rank >= 2) {
                uint64_t twoStep = ONE << ((rank - 2) * 8 + file);
                table[1][sq] |= twoStep;
            }
        }
        
        // Black pawns (isWhite=0, index 0)
        if (rank < 7) { // Can move down (rank increases)
            uint64_t oneStep = ONE << ((rank + 1) * 8 + file);
            table[0][sq] = oneStep;
            
            // Two-step from starting rank (rank 1 = row 7)
            if (rank == 1 && rank <= 5) {
                uint64_t twoStep = ONE << ((rank + 2) * 8 + file);
                table[0][sq] |= twoStep;
            }
        }
    }

    return table;
}();

inline constexpr U64 getPawnForwardPushes(int8_t squareIndex, bool isWhite, U64 occupancy) noexcept {
	// Usa lookup table per target squares, poi filtra con occupancy
	// PAWN_PUSH_TARGETS[isWhite][sq] contiene 1-step e possibile 2-step
	const int colorIndex = isWhite ? 1 : 0;
	U64 targets = PAWN_PUSH_TARGETS[colorIndex][squareIndex];
	
	if (!targets) [[unlikely]] return 0ULL; // No valid pushes (promotion rank già gestito)
	
	// One-step square: sempre il primo bit
	const int8_t rank = rankOf(squareIndex);
	const int8_t file = fileOf(squareIndex);
	const int8_t oneStepRank = rank + (isWhite ? -1 : 1);
	const U64 oneStepBit = ONE << (oneStepRank * 8 + file);
	
	// Se one-step è bloccato, nessuna mossa possibile
	if (occupancy & oneStepBit) return 0ULL;
	
	// Filtra targets con occupancy: rimuovi square occupate
	U64 result = oneStepBit; // one-step è sempre valido se arriviamo qui
	
	// Two-step: solo se presenti in targets E one-step era libero E two-step è libero
	const U64 twoStepBit = targets & ~oneStepBit;
	if (twoStepBit && !(occupancy & twoStepBit)) {
		result |= twoStepBit;
	}
	
	return result;
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


// ==================== UTILS ====================
// inline U64 ray(int8_t file, int8_t rank, int8_t deltaFile, int8_t deltaRank, U64 occupancy);


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
