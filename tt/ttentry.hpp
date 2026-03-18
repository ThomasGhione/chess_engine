#pragma once

#include <cstdint>
#include <cstddef>

#include "../board/board.hpp"
#include "zobrist.hpp"

namespace engine {

// Optimization: compact entry layout to improve cache behavior
// key16(2) + score(2) + depth(1) + age(1) + flag(1) + padding(1) = 8 bytes minimum
// We use a wider key to reduce collisions: key(4) + score(2) + depth(1) + age(1) + flag(1) + pad(3) = 12 bytes
struct TTEntry {
    uint64_t key;   // 64-bit Zobrist key
    int16_t score;  // score in centipawns (±32k range sufficient with mate detection)
    uint8_t depth;  // search depth (0-255 sufficient)
    uint8_t age;    // generation/age for replacement policy
    uint8_t flag;   // INVALID / EXACT / LOWERBOUND / UPPERBOUND
    [[maybe_unused]] uint8_t padding[3]; // align to 16 bytes

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

// Global transposition table + age/generation counter
struct TTGlobal {
    TTEntry table[TTEntry::TABLE_SIZE];
    uint8_t generation = 0;
};

inline TTGlobal& globalTTData() noexcept {
    static TTGlobal data;
    return data;
}

inline uint64_t computeHashKey(const chess::Board& board) noexcept {
    // Legacy compatibility wrapper: runtime hashing is centralized in tt/zobrist.hpp
    return zobrist::computeHashKey(board);
}

} // namespace engine

