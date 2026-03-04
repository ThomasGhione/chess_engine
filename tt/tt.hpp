#ifndef TT_TRANSPOSITION_TABLE_HPP
#define TT_TRANSPOSITION_TABLE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "zobrist.hpp"

namespace tt {

class TranspositionTable {
public:
    struct Entry {
        // 16-byte layout:
        // key(8) + score(4) + bestMove(2) + packed(depth/age/flag)(2)
        // 4 entries = 64 bytes (1 cache line).
        uint64_t key = 0ULL;
        int32_t score = 0;
        uint16_t bestMove = 0;
        uint16_t packed = 0;

        enum Flag : uint8_t {
            INVALID = 0,
            EXACT,
            LOWERBOUND,
            UPPERBOUND
        };

        static constexpr uint8_t DEPTH_BITS = 6;      // 0..63 plies
        static constexpr uint8_t FLAG_BITS = 2;       // 0..3
        static constexpr uint8_t AGE_BITS = 8;        // 0..255 generations
        static constexpr uint8_t MAX_DEPTH = (1u << DEPTH_BITS) - 1u;

        static constexpr uint16_t DEPTH_MASK = static_cast<uint16_t>((1u << DEPTH_BITS) - 1u);
        static constexpr uint8_t FLAG_SHIFT = DEPTH_BITS;
        static constexpr uint16_t FLAG_MASK = static_cast<uint16_t>((1u << FLAG_BITS) - 1u);
        static constexpr uint8_t AGE_SHIFT = DEPTH_BITS + FLAG_BITS;

        inline uint8_t depth() const noexcept {
            return static_cast<uint8_t>(packed & DEPTH_MASK);
        }

        inline uint8_t flag() const noexcept {
            return static_cast<uint8_t>((packed >> FLAG_SHIFT) & FLAG_MASK);
        }

        inline uint8_t age() const noexcept {
            return static_cast<uint8_t>(packed >> AGE_SHIFT);
        }

        inline void setPacked(uint8_t depthValue, uint8_t ageValue, uint8_t flagValue) noexcept {
            // depthValue is clamped by caller in hot paths (store/probe wrappers).
            const uint16_t d = static_cast<uint16_t>(depthValue);
            const uint16_t a = static_cast<uint16_t>(ageValue);
            const uint16_t f = static_cast<uint16_t>(flagValue & FLAG_MASK);
            packed = static_cast<uint16_t>(d | (f << FLAG_SHIFT) | (a << AGE_SHIFT));
        }

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

    static constexpr std::size_t BUCKET_COUNT = 1u << 20;
    static constexpr std::size_t ENTRIES_PER_BUCKET = 4;
    static constexpr std::size_t TABLE_SIZE = BUCKET_COUNT * ENTRIES_PER_BUCKET;
    static constexpr int32_t ADJUSTMENT = 50;

    static_assert(sizeof(Entry) == 16, "TT entry must be 16 bytes");
    static_assert((sizeof(Entry) * ENTRIES_PER_BUCKET) == 64, "Each bucket should be exactly one cache line");
    static_assert((BUCKET_COUNT & (BUCKET_COUNT - 1)) == 0, "BUCKET_COUNT must be power of 2");

    TranspositionTable()
        : table_(std::make_unique<TableStorage>())
        , generation_(0) {
        clear();
    }

    inline void prefetch(uint64_t key) noexcept;
    inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore) noexcept;
    inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept;

    inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore, uint16_t& outBestMove) noexcept;
    inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept;

    inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) noexcept;
    inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) noexcept;

    inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore, uint16_t& outBestMove) noexcept;
    inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag, uint16_t bestMove) noexcept;

    inline void incrementGeneration() noexcept { ++generation_; }
    inline void clear() noexcept;

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;
    TranspositionTable(TranspositionTable&&) = default;
    TranspositionTable& operator=(TranspositionTable&&) = default;

private:
    struct alignas(64) TableStorage {
        std::array<Entry, TABLE_SIZE> entries{};
    };

    std::unique_ptr<TableStorage> table_;
    uint8_t generation_;

    inline Entry* data() noexcept { return table_->entries.data(); }
    inline const Entry* data() const noexcept { return table_->entries.data(); }

    [[nodiscard]] static inline uint8_t clampDepth(uint8_t depth) noexcept {
        return (depth <= Entry::MAX_DEPTH) ? depth : Entry::MAX_DEPTH;
    }

    [[nodiscard]] static inline int32_t clampI64ToI32(int64_t value) noexcept {
        if (value > static_cast<int64_t>(INT32_MAX - 1)) return INT32_MAX - 1;
        if (value < static_cast<int64_t>(INT32_MIN + 1)) return INT32_MIN + 1;
        return static_cast<int32_t>(value);
    }
};

inline void TranspositionTable::prefetch(uint64_t key) noexcept {
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    const Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);
    __builtin_prefetch(bucket, 0, 3);
}

inline bool TranspositionTable::probe(uint64_t key, uint8_t depth,
                                      int32_t alpha, int32_t beta, int32_t& outScore) noexcept {
    const uint8_t neededDepth = clampDepth(depth);
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    const Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        const Entry& entry = bucket[i];
        if (entry.key != key) continue;

        const uint8_t flag = entry.flag();
        if (flag == Entry::INVALID) continue;
        if (entry.depth() < neededDepth) continue;

        const int32_t score = entry.score;
        if (flag == Entry::EXACT
            || (flag == Entry::LOWERBOUND && score >= beta)
            || (flag == Entry::UPPERBOUND && score <= alpha)) {
            outScore = score;
            return true;
        }
        return false;
    }

    return false;
}

inline bool TranspositionTable::probe(uint64_t key, uint8_t depth,
                                      int32_t alpha, int32_t beta, int32_t& outScore, uint16_t& outBestMove) noexcept {
    const uint8_t neededDepth = clampDepth(depth);
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    const Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        const Entry& entry = bucket[i];
        if (entry.key != key) continue;

        const uint8_t flag = entry.flag();
        if (flag == Entry::INVALID) continue;

        outBestMove = entry.bestMove;
        if (entry.depth() < neededDepth) return false;

        const int32_t score = entry.score;
        if (flag == Entry::EXACT
            || (flag == Entry::LOWERBOUND && score >= beta)
            || (flag == Entry::UPPERBOUND && score <= alpha)) {
            outScore = score;
            return true;
        }
        return false;
    }

    outBestMove = 0;
    return false;
}

inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept {
    const uint8_t storedDepth = clampDepth(depth);
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);

    Entry* replaceEntry = &bucket[0];
    Entry* emptyEntry = nullptr;
    int bestReplaceScore = INT32_MIN;

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        Entry& entry = bucket[i];
        const uint8_t entryFlag = entry.flag();

        if (entryFlag == Entry::INVALID) {
            if (emptyEntry == nullptr) emptyEntry = &entry;
            continue;
        }

        if (entry.key == key) {
            if (storedDepth >= entry.depth() || flag == Entry::EXACT) {
                const uint16_t keepBestMove = entry.bestMove;
                entry.key = key;
                entry.score = score;
                entry.bestMove = keepBestMove;
                entry.setPacked(storedDepth, generation_, flag);
            }
            return;
        }

        const int ageDiff = static_cast<int>(static_cast<uint8_t>(generation_ - entry.age()));
        const int replaceScore = (ageDiff << 8) - (static_cast<int>(entry.depth()) << 2);
        if (replaceScore > bestReplaceScore) {
            bestReplaceScore = replaceScore;
            replaceEntry = &entry;
        }
    }

    Entry* const target = (emptyEntry != nullptr) ? emptyEntry : replaceEntry;
    target->key = key;
    target->score = score;
    target->bestMove = 0;
    target->setPacked(storedDepth, generation_, flag);
}

inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept {
    const uint8_t storedDepth = clampDepth(depth);
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);

    Entry* replaceEntry = &bucket[0];
    Entry* emptyEntry = nullptr;
    int bestReplaceScore = INT32_MIN;

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        Entry& entry = bucket[i];
        const uint8_t entryFlag = entry.flag();

        if (entryFlag == Entry::INVALID) {
            if (emptyEntry == nullptr) emptyEntry = &entry;
            continue;
        }

        if (entry.key == key) {
            if (storedDepth >= entry.depth() || flag == Entry::EXACT) {
                entry.key = key;
                entry.score = score;
                entry.bestMove = bestMove;
                entry.setPacked(storedDepth, generation_, flag);
            }
            return;
        }

        const int ageDiff = static_cast<int>(static_cast<uint8_t>(generation_ - entry.age()));
        const int replaceScore = (ageDiff << 8) - (static_cast<int>(entry.depth()) << 2);
        if (replaceScore > bestReplaceScore) {
            bestReplaceScore = replaceScore;
            replaceEntry = &entry;
        }
    }

    Entry* const target = (emptyEntry != nullptr) ? emptyEntry : replaceEntry;
    target->key = key;
    target->score = score;
    target->bestMove = bestMove;
    target->setPacked(storedDepth, generation_, flag);
}

inline bool TranspositionTable::probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) noexcept {
    const int32_t alpha32 = clampI64ToI32(alpha - ADJUSTMENT);
    const int32_t beta32 = clampI64ToI32(beta + ADJUSTMENT);

    int32_t score32 = 0;
    if (probe(key, depth, alpha32, beta32, score32)) {
        outScore = static_cast<int64_t>(score32);
        return true;
    }
    return false;
}

inline bool TranspositionTable::probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta,
                                      int64_t& outScore, uint16_t& outBestMove) noexcept {
    const int32_t alpha32 = clampI64ToI32(alpha - ADJUSTMENT);
    const int32_t beta32 = clampI64ToI32(beta + ADJUSTMENT);

    int32_t score32 = 0;
    if (probe(key, depth, alpha32, beta32, score32, outBestMove)) {
        outScore = static_cast<int64_t>(score32);
        return true;
    }
    return false;
}

inline void TranspositionTable::store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) noexcept {
    store(key, depth, clampI64ToI32(score), flag);
}

inline void TranspositionTable::store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag, uint16_t bestMove) noexcept {
    store(key, depth, clampI64ToI32(score), flag, bestMove);
}

inline void TranspositionTable::clear() noexcept {
    std::fill_n(data(), TABLE_SIZE, Entry{});
}

inline constexpr TranspositionTable::Entry::Flag
determineFlag(int64_t score, int64_t alphaOrig, int64_t beta) noexcept {
    if (score <= alphaOrig) return TranspositionTable::Entry::UPPERBOUND;
    if (score >= beta) return TranspositionTable::Entry::LOWERBOUND;
    return TranspositionTable::Entry::EXACT;
}

static_assert(determineFlag(100, 50, 200) == TranspositionTable::Entry::EXACT, "determineFlag logic error");
static_assert(determineFlag(40, 50, 200) == TranspositionTable::Entry::UPPERBOUND, "determineFlag logic error");
static_assert(determineFlag(250, 50, 200) == TranspositionTable::Entry::LOWERBOUND, "determineFlag logic error");

} // namespace tt

#endif // TT_TRANSPOSITION_TABLE_HPP
