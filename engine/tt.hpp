#ifndef ENGINE_TT_HPP
#define ENGINE_TT_HPP

#include <cstdint>
#include <cstddef>

#include "../board/board.hpp"

namespace engine {

// Optimization: compact entry layout to improve cache behavior
// key16(2) + score(2) + depth(1) + age(1) + flag(1) + padding(1) = 8 bytes minimum
// We use a wider key to reduce collisions: key(4) + score(2) + depth(1) + age(1) + flag(1) + pad(3) = 12 bytes
struct TTEntry {
    uint64_t key;   // 64-bit Zobrist key
    int16_t  score;   // score in centipawns (±32k range sufficient with mate detection)
    uint8_t  depth;   // search depth (0-255 sufficient)
    uint8_t  age  ;   // generation/age for replacement policy
    uint8_t  flag;   // INVALID / EXACT / LOWERBOUND / UPPERBOUND
    [[maybe_unused]] uint8_t  padding[3]; // align to 16 bytes

    enum Flag : uint8_t {
        INVALID = 0,  // empty/invalid entry
        EXACT,
        LOWERBOUND,
        UPPERBOUND
    };

    // Bucket-based TT: 4 entries per bucket to reduce collisions (cache line = 64 bytes, bucket ~48 bytes)
    static constexpr std::size_t BUCKET_COUNT = 1u << 20; // 2^20 buckets = 4M buckets (table size = 16M entries)
    static constexpr std::size_t ENTRIES_PER_BUCKET = 4;
    static constexpr std::size_t TABLE_SIZE = BUCKET_COUNT * ENTRIES_PER_BUCKET;
    
    static constexpr int32_t ADJUSTMENT = 50; 

    //TTEntry() = default;
    //TTEntry(uint64_t k, uint8_t d, int16_t s, uint8_t f, uint8_t a)
    //    : key(k), score(s), depth(d), age(a), flag(f) {}
};

// Transposition table globale + age/generation counter
struct TTGlobal {
    TTEntry table[TTEntry::TABLE_SIZE];
    uint8_t generation = 0;
};

inline TTGlobal& globalTTData() {
    static TTGlobal data;
    return data;
}

inline TTEntry* globalTT() {
    return globalTTData().table;
}

inline void incrementTTGeneration() {
    ++globalTTData().generation;
}

inline uint8_t getCurrentGeneration() {
    return globalTTData().generation;
}

// Zobrist: 16 pezzi x 64 caselle + side-to-move, castling, en-passant.
// Tutte le chiavi sono pre-calcolate a compile time.
namespace detail {

struct XorShift64 {
    uint64_t state;
    constexpr explicit XorShift64(uint64_t seed) : state(seed) {}
    constexpr uint64_t next() {
        uint64_t x = state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state = x;
        return x * 0x2545F4914F6CDD1DULL;
    }
};

struct ZobristTables {
    uint64_t piece[16][64];
    uint64_t sideToMove;
    uint64_t castling[16];
    uint64_t enPassant[8];
};

constexpr ZobristTables makeZobristTables() {
    ZobristTables t{};
    XorShift64 rng(0x123456789ABCDEF0ULL);

    // Pezzi (16 indici: tipi+colori) x 64 caselle
    for (int p = 0; p < 16; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            t.piece[p][sq] = rng.next();
        }
    }

    // Side to move
    t.sideToMove = rng.next();

    // Castling 0..15 (bitmask KQkq)
    for (int i = 0; i < 16; ++i) {
        t.castling[i] = rng.next();
    }

    // En-passant file 0..7
    for (int f = 0; f < 8; ++f) {
        t.enPassant[f] = rng.next();
    }

    return t;
}

inline constexpr ZobristTables ZOBRIST = makeZobristTables();

} // namespace detail

inline void xorPiecesFromBitboard(uint64_t bb, uint8_t idx, uint64_t& hashKey) noexcept {
    while (bb) {
        const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bb));
        bb &= (bb - 1);
        hashKey ^= detail::ZOBRIST.piece[idx][sq];
    }
}

inline uint64_t computeHashKey(const chess::Board& board) {
    uint64_t hashKey = 0ULL;

    // White pieces (color index 0) - indici 1..6
    xorPiecesFromBitboard(board.pawns_bb[0],   1, hashKey);
    xorPiecesFromBitboard(board.knights_bb[0], 2, hashKey);
    xorPiecesFromBitboard(board.bishops_bb[0], 3, hashKey);
    xorPiecesFromBitboard(board.rooks_bb[0],   4, hashKey);
    xorPiecesFromBitboard(board.queens_bb[0],  5, hashKey);
    xorPiecesFromBitboard(board.kings_bb[0],   6, hashKey);

    // Black pieces (color index 1) - indici 9..14
    xorPiecesFromBitboard(board.pawns_bb[1],   9, hashKey);
    xorPiecesFromBitboard(board.knights_bb[1], 10, hashKey);
    xorPiecesFromBitboard(board.bishops_bb[1], 11, hashKey);
    xorPiecesFromBitboard(board.rooks_bb[1],   12, hashKey);
    xorPiecesFromBitboard(board.queens_bb[1],  13, hashKey);
    xorPiecesFromBitboard(board.kings_bb[1],   14, hashKey);

    // Side to move: XOR if black to move (branchless)
    const uint64_t stmMask = static_cast<uint64_t>(-(board.getActiveColor() == chess::Board::BLACK));
    hashKey ^= detail::ZOBRIST.sideToMove & stmMask;

    // Castling rights: use the Castle bitmask directly (0-15)
    const uint8_t castlingMask = 
        (board.getCastle(0) ? 1u : 0u) |
        (board.getCastle(1) ? 2u : 0u) |
        (board.getCastle(2) ? 4u : 0u) |
        (board.getCastle(3) ? 8u : 0u);
    hashKey ^= detail::ZOBRIST.castling[castlingMask];

    // En-passant: single EP square
    const chess::Coords epSquare = board.getEnPassant();
    const uint64_t epMask = static_cast<uint64_t>(-static_cast<int64_t>(chess::Coords::isInBounds(epSquare)));
    hashKey ^= detail::ZOBRIST.enPassant[epSquare.file()] & epMask;

    return hashKey;
}

// Prefetch a TT bucket to reduce cache misses (call before probe/store)
inline void prefetchTT(uint64_t key) {
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (TTEntry::BUCKET_COUNT - 1);
    const TTEntry* bucket = &globalTT()[bucketIndex * TTEntry::ENTRIES_PER_BUCKET];
    __builtin_prefetch(bucket, 0, 3); // Read prefetch, high temporal locality
}

// Store with replacement policy: prefer higher depth and use age as a tie-breaker
inline void storeTTEntry(TTEntry* ttTable,
                         uint64_t key,
                         uint8_t depth,
                         int16_t score,
                         uint8_t flag) {

    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (TTEntry::BUCKET_COUNT - 1);
    TTEntry* bucket = &ttTable[bucketIndex * TTEntry::ENTRIES_PER_BUCKET];
    
    const uint8_t generation = getCurrentGeneration();

    TTEntry* replaceEntry = &bucket[0];
    int bestReplaceScore = -1000000;

    // Search for an existing entry or the best candidate to replace
    for (std::size_t i = 0; i < TTEntry::ENTRIES_PER_BUCKET; ++i) {
        TTEntry& entry = bucket[i];
        
    // If the same key is found, update it (prefer deeper entries)
        if (entry.key == key) {
            if (depth >= entry.depth || flag == TTEntry::EXACT) {
                entry.depth = depth;
                entry.score = score;
                entry.flag  = flag;
                entry.age   = generation;
            }
            return;
        }

    // Compute replacement score: prefer older entries or entries with smaller depth
        const int ageDiff = static_cast<int>(generation - entry.age) & 0xFF;
        const int replaceScore = (ageDiff * 256) - static_cast<int>(entry.depth) * 4;
        
        if (replaceScore > bestReplaceScore) {
            bestReplaceScore = replaceScore;
            replaceEntry = &entry;
        }
    }

    // Replace the selected entry
    replaceEntry->key = key;
    replaceEntry->depth = depth;
    replaceEntry->score = score;
    replaceEntry->flag  = flag;
    replaceEntry->age   = generation;
}

// Optimized probe using bucket search
inline bool probeTT(const TTEntry* ttTable,
                    uint64_t key,
                    uint8_t depth,
                    int16_t alpha,
                    int16_t beta,
                    int16_t& outScore) {

    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (TTEntry::BUCKET_COUNT - 1);
    const TTEntry* bucket = &ttTable[bucketIndex * TTEntry::ENTRIES_PER_BUCKET];
    
    // Cerca in tutte le entry del bucket
    for (std::size_t i = 0; i < TTEntry::ENTRIES_PER_BUCKET; ++i) {
        const TTEntry& entry = bucket[i];
        
        if (entry.flag == TTEntry::INVALID) continue;  // empty entry, skip
        if (entry.key != key) continue;                // different key
        if (entry.depth < depth) continue;             // insufficient depth

        const int16_t score = entry.score;
        
        switch (entry.flag) {
            case TTEntry::EXACT:
                outScore = score;
                return true;
            case TTEntry::LOWERBOUND:
                if (score >= beta) {
                    outScore = score;
                    return true;
                }
                break;
            case TTEntry::UPPERBOUND:
                if (score <= alpha) {
                    outScore = score;
                    return true;
                }
                break;
        }
        
        // Entry found but not usable for cutoff
        return false;
    }
    
    return false;
}

} // namespace engine

#endif // ENGINE_TT_HPP
