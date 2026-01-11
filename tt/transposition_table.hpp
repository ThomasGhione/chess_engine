#ifndef TT_TRANSPOSITION_TABLE_HPP
#define TT_TRANSPOSITION_TABLE_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include <memory>
#include <algorithm>
#include "zobrist.hpp"

namespace tt {
    class TranspositionTable {
    public:
        // Nested Entry type
        struct Entry {
            uint64_t key;       // [0-7]   Hash key for position
            int32_t  score;     // [8-11]  Evaluation score
            uint8_t  depth;     // [12]    Search depth
            uint8_t  age;       // [13]    Generation counter
            uint8_t  flag;      // [14]    Entry type (EXACT/LOWERBOUND/UPPERBOUND)
            uint8_t  padding;   // [15]    Alignment padding
            
            // Total: 16 bytes (cache-line friendly, 4 entries = 64 bytes = 1 cache line)

            enum Flag : uint8_t {
                INVALID = 0,
                EXACT,
                LOWERBOUND,
                UPPERBOUND
            };
        };

        // Configurazione
        static constexpr std::size_t BUCKET_COUNT = 1u << 20;
        static constexpr std::size_t ENTRIES_PER_BUCKET = 4;
        static constexpr std::size_t TABLE_SIZE = BUCKET_COUNT * ENTRIES_PER_BUCKET;
        static constexpr int32_t ADJUSTMENT = 50;

        // Validazioni compile-time
        static_assert(sizeof(Entry) == 16, "Entry must be exactly 16 bytes");
        static_assert(alignof(Entry) == 8, "Entry must be 8-byte aligned for cache efficiency");
        static_assert((BUCKET_COUNT & (BUCKET_COUNT - 1)) == 0, "BUCKET_COUNT must be power of 2");

        // Costruttore pubblico (per istanza in Engine)
        // Alloca table_ dinamicamente (heap) per evitare stack overflow
        TranspositionTable() 
            : table_(std::make_unique<std::array<Entry, TABLE_SIZE>>()), generation_(0) {}

        // Operazioni principali con int32_t (native API)
        inline void prefetch(uint64_t key) noexcept;
        inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore) noexcept;
        inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept;

        // Overload per int64_t (conversioni gestite internamente)
        inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) noexcept;
        inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) noexcept;

        // Generation management
        void incrementGeneration() { ++generation_; }
        uint8_t getCurrentGeneration() const { return generation_; }

        // Utility
        void clear();
        size_t getMemoryUsage() const { return sizeof(*table_); }

        // Prevent copying, allow moving
        TranspositionTable(const TranspositionTable&) = delete;
        TranspositionTable& operator=(const TranspositionTable&) = delete;
        TranspositionTable(TranspositionTable&&) = default;
        TranspositionTable& operator=(TranspositionTable&&) = default;

    private:
        std::unique_ptr<std::array<Entry, TABLE_SIZE>> table_;
        uint8_t generation_;
    };

    // Implementazioni inline dei metodi critici
    inline void TranspositionTable::prefetch(uint64_t key) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        const Entry* bucket = &(*table_)[bucketIndex * ENTRIES_PER_BUCKET];
        
        // Prefetch for read (0), high temporal locality (3)
        // Brings 64-byte cache line (4 entries) into L1 cache
        // ~200 cycle latency reduction on cache miss
        __builtin_prefetch(bucket, 0, 3);
    }

    inline bool TranspositionTable::probe(uint64_t key, uint8_t depth,
                                          int32_t alpha, int32_t beta, int32_t& outScore) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        const Entry* bucket = &(*table_)[bucketIndex * ENTRIES_PER_BUCKET];

        for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            const Entry& entry = bucket[i];

            if (entry.flag == Entry::INVALID) continue;
            if (entry.key != key) continue;
            if (entry.depth < depth) continue;

            const int32_t score = entry.score;

            switch (entry.flag) {
                case Entry::EXACT:
                    outScore = score;
                    return true;
                case Entry::LOWERBOUND:
                    if (score >= beta) {
                        outScore = score;
                        return true;
                    }
                    break;
                case Entry::UPPERBOUND:
                    if (score <= alpha) {
                        outScore = score;
                        return true;
                    }
                    break;
            }

            return false;
        }

        return false;
    }

    inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        Entry* bucket = &(*table_)[bucketIndex * ENTRIES_PER_BUCKET];

        Entry* replaceEntry = &bucket[0];
        int bestReplaceScore = -1000000;

        for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            Entry& entry = bucket[i];

            if (entry.key == key) {
                if (depth >= entry.depth || flag == Entry::EXACT) {
                    entry.depth = depth;
                    entry.score = score;
                    entry.flag  = flag;
                    entry.age   = generation_;
                }
                return;
            }

            const int ageDiff = static_cast<int>(generation_ - entry.age) & 0xFF;
            const int replaceScore = (ageDiff * 256) - static_cast<int>(entry.depth) * 4;

            if (replaceScore > bestReplaceScore) {
                bestReplaceScore = replaceScore;
                replaceEntry = &entry;
            }
        }

        replaceEntry->key = key;
        replaceEntry->depth = depth;
        replaceEntry->score = score;
        replaceEntry->flag  = flag;
        replaceEntry->age   = generation_;
    }

    // Overload int64_t → int32_t (conversioni gestite da TT)
    inline bool TranspositionTable::probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) noexcept {
        const int32_t alpha32 = static_cast<int32_t>(
            std::max<int64_t>(alpha - ADJUSTMENT, INT32_MIN + 1));
        const int32_t beta32 = static_cast<int32_t>(
            std::min<int64_t>(beta + ADJUSTMENT, INT32_MAX - 1));

        int32_t score32 = 0;
        if (probe(key, depth, alpha32, beta32, score32)) {
            outScore = static_cast<int64_t>(score32);
            return true;
        }
        return false;
    }

    inline void TranspositionTable::store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) noexcept {
        const int32_t score32 = static_cast<int32_t>(
            std::max<int64_t>(std::min<int64_t>(score, INT32_MAX - 1), INT32_MIN + 1));
        store(key, depth, score32, flag);
    }

    inline void TranspositionTable::clear() {
        for (Entry& entry : *table_) {
            entry.flag = Entry::INVALID;
        }
    }

    // Helper per determinare flag TT (riduce duplicazione codice in Engine)
    inline constexpr TranspositionTable::Entry::Flag 
    determineFlag(int64_t score, int64_t alphaOrig, int64_t beta) noexcept {
        if (score <= alphaOrig) return TranspositionTable::Entry::UPPERBOUND;
        if (score >= beta) return TranspositionTable::Entry::LOWERBOUND;
        return TranspositionTable::Entry::EXACT;
    }

    // Compile-time validation di determineFlag
    static_assert(determineFlag(100, 50, 200) == TranspositionTable::Entry::EXACT, "determineFlag logic error");
    static_assert(determineFlag(40, 50, 200) == TranspositionTable::Entry::UPPERBOUND, "determineFlag logic error");
    static_assert(determineFlag(250, 50, 200) == TranspositionTable::Entry::LOWERBOUND, "determineFlag logic error");
}

#endif // TT_TRANSPOSITION_TABLE_HPP
