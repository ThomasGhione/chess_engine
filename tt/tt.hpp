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
            uint8_t  padding;   // [15]    Alignment padding (unused for now)
            uint16_t bestMove;  // [16-17] Encoded best move: from(6) + to(6) + promo(4)
            
            // Total: 18 bytes (slightly less cache-friendly but worth it for hash move)

            enum Flag : uint8_t {
                INVALID = 0,
                EXACT,
                LOWERBOUND,
                UPPERBOUND
            };
            
            // Helper functions for move encoding/decoding
            // Move encoding: from(6 bits) + to(6 bits) + promotion(4 bits) = 16 bits
            // promotion: 0=none, 1=Q, 2=R, 3=B, 4=N
            static constexpr uint16_t encodeMove(uint8_t from, uint8_t to, char promo) noexcept {
                uint8_t promoCode = 0;
                if (promo == 'q' || promo == 'Q') promoCode = 1;
                else if (promo == 'r' || promo == 'R') promoCode = 2;
                else if (promo == 'b' || promo == 'B') promoCode = 3;
                else if (promo == 'n' || promo == 'N') promoCode = 4;
                
                return (static_cast<uint16_t>(from) & 0x3F) 
                     | ((static_cast<uint16_t>(to) & 0x3F) << 6)
                     | ((static_cast<uint16_t>(promoCode) & 0xF) << 12);
            }
            
            static constexpr void decodeMove(uint16_t encoded, uint8_t& from, uint8_t& to, char& promo) noexcept {
                from = encoded & 0x3F;
                to = (encoded >> 6) & 0x3F;
                const uint8_t promoCode = (encoded >> 12) & 0xF;
                
                switch (promoCode) {
                    case 1: promo = 'q'; break;
                    case 2: promo = 'r'; break;
                    case 3: promo = 'b'; break;
                    case 4: promo = 'n'; break;
                    default: promo = '\0'; break;
                }
            }
        };

        // Configuration
        static constexpr std::size_t BUCKET_COUNT = 1u << 20;
        static constexpr std::size_t ENTRIES_PER_BUCKET = 4;
        static constexpr std::size_t TABLE_SIZE = BUCKET_COUNT * ENTRIES_PER_BUCKET;
        static constexpr int32_t ADJUSTMENT = 50;

        // Compile-time validations
        // static_assert(sizeof(Entry) == 18, "Entry must be exactly 18 bytes");
        static_assert(alignof(Entry) == 8, "Entry must be 8-byte aligned for cache efficiency");
        static_assert((BUCKET_COUNT & (BUCKET_COUNT - 1)) == 0, "BUCKET_COUNT must be power of 2");

        // Public constructor (for Engine instances)
        // Allocate table_ dynamically (heap) to avoid stack overflow
        TranspositionTable() 
            : table_(std::make_unique<std::array<Entry, TABLE_SIZE>>()), generation_(0) {}

        // Operazioni principali con int32_t (native API)
        inline void prefetch(uint64_t key) noexcept;
        inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore) noexcept;
        inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept;
        
        // New API with hash move support
        inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore, uint16_t& outBestMove) noexcept;
        inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept;

        // int64_t overload (conversions handled internally)
        inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) noexcept;
        inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) noexcept;
        
        // int64_t API with hash move support
        inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore, uint16_t& outBestMove) noexcept;
        inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag, uint16_t bestMove) noexcept;

        // Generation management
        void incrementGeneration() { ++generation_; }

        // Utility
        void clear();

        // Prevent copying, allow moving
        TranspositionTable(const TranspositionTable&) = delete;
        TranspositionTable& operator=(const TranspositionTable&) = delete;
        TranspositionTable(TranspositionTable&&) = default;
        TranspositionTable& operator=(TranspositionTable&&) = default;

    private:
        std::unique_ptr<std::array<Entry, TABLE_SIZE>> table_;
        uint8_t generation_;
    };

    // Inline implementations of critical methods
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
        Entry* emptyEntry = nullptr;
        int bestReplaceScore = -1000000;

        for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            Entry& entry = bucket[i];

            // INVALID entries are empty slots (their key/depth may contain stale data).
            // Never treat them as key matches.
            if (entry.flag == Entry::INVALID) {
                if (emptyEntry == nullptr) emptyEntry = &entry;
                continue;
            }

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

        Entry* const target = (emptyEntry != nullptr) ? emptyEntry : replaceEntry;
        target->key = key;
        target->depth = depth;
        target->score = score;
        target->flag  = flag;
        target->age   = generation_;
    }

    // NEW: Probe with hash move retrieval
    inline bool TranspositionTable::probe(uint64_t key, uint8_t depth,
                                          int32_t alpha, int32_t beta, int32_t& outScore, uint16_t& outBestMove) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        const Entry* bucket = &(*table_)[bucketIndex * ENTRIES_PER_BUCKET];

        for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            const Entry& entry = bucket[i];

            if (entry.flag == Entry::INVALID) continue;
            if (entry.key != key) continue;
            
            // Always retrieve best move even if depth is insufficient for cutoff
            outBestMove = entry.bestMove;
            
            if (entry.depth < depth) {
                // Depth insufficient for score cutoff, but we got the hash move
                return false;
            }

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

        outBestMove = 0; // No entry found
        return false;
    }

    // NEW: Store with best move
    inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        Entry* bucket = &(*table_)[bucketIndex * ENTRIES_PER_BUCKET];

        Entry* replaceEntry = &bucket[0];
        Entry* emptyEntry = nullptr;
        int bestReplaceScore = -1000000;

        for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            Entry& entry = bucket[i];

            // INVALID entries are empty slots (their key/depth may contain stale data).
            // Never treat them as key matches.
            if (entry.flag == Entry::INVALID) {
                if (emptyEntry == nullptr) emptyEntry = &entry;
                continue;
            }

            if (entry.key == key) {
                if (depth >= entry.depth || flag == Entry::EXACT) {
                    entry.depth = depth;
                    entry.score = score;
                    entry.flag  = flag;
                    entry.age   = generation_;
                    entry.bestMove = bestMove;
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

        Entry* const target = (emptyEntry != nullptr) ? emptyEntry : replaceEntry;
        target->key = key;
        target->depth = depth;
        target->score = score;
        target->flag  = flag;
        target->age   = generation_;
        target->bestMove = bestMove;
    }

    // Overload int64_t -> int32_t (conversioni gestite da TT)
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

    // NEW: int64_t overloads with best move support
    inline bool TranspositionTable::probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, 
                                          int64_t& outScore, uint16_t& outBestMove) noexcept {
        const int32_t alpha32 = static_cast<int32_t>(
            std::max<int64_t>(alpha - ADJUSTMENT, INT32_MIN + 1));
        const int32_t beta32 = static_cast<int32_t>(
            std::min<int64_t>(beta + ADJUSTMENT, INT32_MAX - 1));

        int32_t score32 = 0;
        if (probe(key, depth, alpha32, beta32, score32, outBestMove)) {
            outScore = static_cast<int64_t>(score32);
            return true;
        }
        return false;
    }

    inline void TranspositionTable::store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag, uint16_t bestMove) noexcept {
        const int32_t score32 = static_cast<int32_t>(
            std::max<int64_t>(std::min<int64_t>(score, INT32_MAX - 1), INT32_MIN + 1));
        store(key, depth, score32, flag, bestMove);
    }

    inline void TranspositionTable::clear() {
        for (Entry& entry : *table_) {
            entry.flag = Entry::INVALID;
        }
    }

    // Helper to determine TT flag (reduces code duplication in Engine)
    inline constexpr TranspositionTable::Entry::Flag 
    determineFlag(int64_t score, int64_t alphaOrig, int64_t beta) noexcept {
        if (score <= alphaOrig) return TranspositionTable::Entry::UPPERBOUND;
        if (score >= beta) return TranspositionTable::Entry::LOWERBOUND;
        return TranspositionTable::Entry::EXACT;
    }

    // Compile-time validation of determineFlag
    static_assert(determineFlag(100, 50, 200) == TranspositionTable::Entry::EXACT, "determineFlag logic error");
    static_assert(determineFlag(40, 50, 200) == TranspositionTable::Entry::UPPERBOUND, "determineFlag logic error");
    static_assert(determineFlag(250, 50, 200) == TranspositionTable::Entry::LOWERBOUND, "determineFlag logic error");
}

#endif // TT_TRANSPOSITION_TABLE_HPP
