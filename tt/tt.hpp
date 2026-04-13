#pragma once

#include <algorithm>
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <string_view>

#if defined(__linux__)
#include <sys/mman.h>
#endif

#include "zobrist.hpp"

class TranspositionTable {

public:
    enum class HugePageMode : uint8_t {
        Auto = 0,
        On,
        Off
    };

    struct Entry {
        // 16-byte layout:
        // key(8) + payload(8)
        // payload packing:
        //   bits  0..15  -> packed(depth/age/flag)
        //   bits 16..31  -> bestMove
        //   bits 32..63  -> score (int32_t)
        // 4 entries = 64 bytes (1 cache line).
        uint64_t key = 0ULL;
        uint64_t payload = 0ULL;

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

        static constexpr uint16_t packedMeta(uint8_t depthValue, uint8_t ageValue, uint8_t flagValue) noexcept {
            const uint16_t d = static_cast<uint16_t>(depthValue);
            const uint16_t a = static_cast<uint16_t>(ageValue);
            const uint16_t f = static_cast<uint16_t>(flagValue & FLAG_MASK);
            return static_cast<uint16_t>(d | (f << FLAG_SHIFT) | (a << AGE_SHIFT));
        }

        static constexpr uint16_t packedFromPayload(uint64_t payloadValue) noexcept {
            return static_cast<uint16_t>(payloadValue & 0xFFFFULL);
        }

        static constexpr uint8_t depthFromPayload(uint64_t payloadValue) noexcept {
            return static_cast<uint8_t>(packedFromPayload(payloadValue) & DEPTH_MASK);
        }

        static constexpr uint8_t flagFromPayload(uint64_t payloadValue) noexcept {
            return static_cast<uint8_t>((packedFromPayload(payloadValue) >> FLAG_SHIFT) & FLAG_MASK);
        }

        static constexpr uint8_t ageFromPayload(uint64_t payloadValue) noexcept {
            return static_cast<uint8_t>(packedFromPayload(payloadValue) >> AGE_SHIFT);
        }

        static constexpr uint16_t bestMoveFromPayload(uint64_t payloadValue) noexcept {
            return static_cast<uint16_t>((payloadValue >> 16) & 0xFFFFULL);
        }

        static constexpr int32_t scoreFromPayload(uint64_t payloadValue) noexcept {
            return static_cast<int32_t>(static_cast<uint32_t>(payloadValue >> 32));
        }

        static constexpr uint64_t encodePayload(
            int32_t scoreValue,
            uint16_t bestMoveValue,
            uint8_t depthValue,
            uint8_t ageValue,
            uint8_t flagValue) noexcept {
            return (static_cast<uint64_t>(static_cast<uint32_t>(scoreValue)) << 32)
                 | (static_cast<uint64_t>(bestMoveValue) << 16)
                 | static_cast<uint64_t>(packedMeta(depthValue, ageValue, flagValue));
        }

        inline uint8_t depth() const noexcept {
            return depthFromPayload(payload);
        }

        inline uint8_t flag() const noexcept {
            return flagFromPayload(payload);
        }

        inline uint8_t age() const noexcept {
            return ageFromPayload(payload);
        }

        inline uint16_t bestMove() const noexcept {
            return bestMoveFromPayload(payload);
        }

        inline int32_t score() const noexcept {
            return scoreFromPayload(payload);
        }

        inline void setPayload(
            int32_t scoreValue,
            uint16_t bestMoveValue,
            uint8_t depthValue,
            uint8_t ageValue,
            uint8_t flagValue) noexcept {
            payload = encodePayload(scoreValue, bestMoveValue, depthValue, ageValue, flagValue);
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
    static constexpr std::size_t TABLE_BYTES = sizeof(Entry) * TABLE_SIZE;
    static constexpr std::size_t HUGE_PAGE_MIN_BYTES = 32u * 1024u * 1024u;
    static constexpr int32_t ADJUSTMENT = 50;

    static_assert(sizeof(Entry) == 16, "TT entry must be 16 bytes");
    static_assert((sizeof(Entry) * ENTRIES_PER_BUCKET) == 64, "Each bucket should be exactly one cache line");
    static_assert((BUCKET_COUNT & (BUCKET_COUNT - 1)) == 0, "BUCKET_COUNT must be power of 2");

    explicit TranspositionTable(HugePageMode mode = HugePageMode::Auto)
        : table_(nullptr, TableDeleter{})
        , generation_(0)
        , hugePageMode_(resolveHugePageMode(mode))
        , hugePagesBacked_(false) {
        table_ = allocateTable(hugePageMode_, hugePagesBacked_);
        clear();
    }

    inline void prefetch(uint64_t key) noexcept;
    inline bool probeMove(uint64_t key, uint16_t& outBestMove) const noexcept;
    inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore) noexcept;
    inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept;

    inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore, uint16_t& outBestMove) noexcept;
    inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept;

    inline void incrementGeneration() noexcept { ++generation_; }
    inline void clear() noexcept;
    [[nodiscard]] inline HugePageMode hugePageMode() const noexcept { return hugePageMode_; }
    [[nodiscard]] inline bool isHugePageBacked() const noexcept { return hugePagesBacked_; }

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;
    TranspositionTable(TranspositionTable&&) = default;
    TranspositionTable& operator=(TranspositionTable&&) = default;

private:
    struct BucketSeq {
        std::atomic<uint32_t> value{0U};
    };

    struct alignas(64) TableStorage {
        std::array<Entry, TABLE_SIZE> entries{};
        std::array<BucketSeq, BUCKET_COUNT> bucketSeq{};
    };

    enum class AllocationKind : uint8_t {
        Heap = 0,
        MMap
    };

    struct TableDeleter {
        AllocationKind kind = AllocationKind::Heap;
        std::size_t mappedBytes = 0;

        inline void operator()(TableStorage* ptr) const noexcept {
            if (ptr == nullptr) return;
            ptr->~TableStorage();
#if defined(__linux__)
            if (kind == AllocationKind::MMap) {
                (void)::munmap(static_cast<void*>(ptr), mappedBytes);
                return;
            }
#endif
            ::operator delete(static_cast<void*>(ptr), std::align_val_t(alignof(TableStorage)));
        }
    };

    using TablePtr = std::unique_ptr<TableStorage, TableDeleter>;

    TablePtr table_;
    uint8_t generation_;
    HugePageMode hugePageMode_;
    bool hugePagesBacked_;

    inline Entry* data() noexcept { return table_->entries.data(); }
    inline const Entry* data() const noexcept { return table_->entries.data(); }
    inline BucketSeq* seqData() noexcept { return table_->bucketSeq.data(); }
    inline const BucketSeq* seqData() const noexcept { return table_->bucketSeq.data(); }

    struct EntrySnapshot {
        uint64_t key = 0ULL;
        uint64_t payload = 0ULL;
    };

    [[nodiscard]] static inline uint64_t loadEntryKeyAtomic(const Entry& entry) noexcept {
        auto& keyRef = const_cast<uint64_t&>(entry.key);
        return std::atomic_ref<uint64_t>(keyRef).load(std::memory_order_relaxed);
    }

    [[nodiscard]] static inline uint64_t loadEntryPayloadAtomic(const Entry& entry) noexcept {
        auto& payloadRef = const_cast<uint64_t&>(entry.payload);
        return std::atomic_ref<uint64_t>(payloadRef).load(std::memory_order_relaxed);
    }

    static inline void storeEntryKeyAtomic(Entry& entry, uint64_t key) noexcept {
        std::atomic_ref<uint64_t>(entry.key).store(key, std::memory_order_relaxed);
    }

    static inline void storeEntryPayloadAtomic(Entry& entry, uint64_t payload) noexcept {
        std::atomic_ref<uint64_t>(entry.payload).store(payload, std::memory_order_relaxed);
    }

    [[nodiscard]] static inline uint32_t lockBucket(BucketSeq& bucketSeq) noexcept {
        uint32_t expected = bucketSeq.value.load(std::memory_order_relaxed);
        for (;;) {
            while ((expected & 1U) != 0U) {
                expected = bucketSeq.value.load(std::memory_order_acquire);
            }

            const uint32_t desired = expected + 1U; // odd => writer owns lock
            if (bucketSeq.value.compare_exchange_weak(
                    expected,
                    desired,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                return expected;
            }
        }
    }

    static inline void unlockBucket(BucketSeq& bucketSeq, uint32_t lockBase) noexcept {
        bucketSeq.value.store(lockBase + 2U, std::memory_order_release);
    }

    [[nodiscard]] static inline bool readBucketSnapshot(
        const Entry* bucket,
        const BucketSeq& bucketSeq,
        EntrySnapshot (&snapshot)[ENTRIES_PER_BUCKET]) noexcept {
        constexpr int MAX_RETRIES = 8;
        for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
            const uint32_t seqStart = bucketSeq.value.load(std::memory_order_acquire);
            if ((seqStart & 1U) != 0U) {
                continue;
            }

            for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
                snapshot[i].key = loadEntryKeyAtomic(bucket[i]);
                snapshot[i].payload = loadEntryPayloadAtomic(bucket[i]);
            }

            const uint32_t seqEnd = bucketSeq.value.load(std::memory_order_acquire);
            if (seqStart == seqEnd && (seqEnd & 1U) == 0U) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static inline bool iequals(std::string_view lhs, std::string_view rhs) noexcept {
        if (lhs.size() != rhs.size()) return false;
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            char a = lhs[i];
            char b = rhs[i];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
            if (a != b) return false;
        }
        return true;
    }

    [[nodiscard]] static inline HugePageMode hugePageModeFromEnv() noexcept {
        const char* envValue = std::getenv("CHESS_TT_HUGEPAGE");
        if (envValue == nullptr || *envValue == '\0') return HugePageMode::Auto;

        const std::string_view value(envValue);
        if (iequals(value, "on") || iequals(value, "1") || iequals(value, "true") || iequals(value, "force")) {
            return HugePageMode::On;
        }
        if (iequals(value, "off") || iequals(value, "0") || iequals(value, "false")) {
            return HugePageMode::Off;
        }
        return HugePageMode::Auto;
    }

    [[nodiscard]] static inline HugePageMode resolveHugePageMode(HugePageMode requestedMode) noexcept {
        if (requestedMode == HugePageMode::Auto) {
            return hugePageModeFromEnv();
        }
        return requestedMode;
    }

    [[nodiscard]] static inline std::size_t alignUp(std::size_t value, std::size_t alignment) noexcept {
        return (value + alignment - 1u) & ~(alignment - 1u);
    }

    [[nodiscard]] static inline TablePtr allocateHeapTable() {
        void* raw = ::operator new(sizeof(TableStorage), std::align_val_t(alignof(TableStorage)));
        auto* table = ::new (raw) TableStorage();
        return TablePtr(table, TableDeleter{AllocationKind::Heap, 0});
    }

#if defined(__linux__)
    [[nodiscard]] static inline TablePtr makeMappedTable(void* mappedMemory, std::size_t mappedBytes) {
        auto* table = ::new (mappedMemory) TableStorage();
        return TablePtr(table, TableDeleter{AllocationKind::MMap, mappedBytes});
    }

    [[nodiscard]] static inline TablePtr tryAllocateExplicitHugePages(bool& outHugePagesBacked) {
        constexpr std::size_t HugePageBytes = 2u * 1024u * 1024u;
        const std::size_t mapBytes = alignUp(sizeof(TableStorage), HugePageBytes);
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB;
#if defined(MAP_HUGE_2MB)
        flags |= MAP_HUGE_2MB;
#endif
        void* mapped = ::mmap(nullptr, mapBytes, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (mapped == MAP_FAILED) {
            return TablePtr(nullptr, TableDeleter{AllocationKind::Heap, 0});
        }

        outHugePagesBacked = true;
        return makeMappedTable(mapped, mapBytes);
    }

    [[nodiscard]] static inline TablePtr tryAllocateTransparentHugePages(bool& outHugePagesBacked) {
        const std::size_t mapBytes = sizeof(TableStorage);
        void* mapped = ::mmap(nullptr, mapBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped == MAP_FAILED) {
            return TablePtr(nullptr, TableDeleter{AllocationKind::Heap, 0});
        }

#if defined(MADV_HUGEPAGE)
        if (::madvise(mapped, mapBytes, MADV_HUGEPAGE) == 0) {
            outHugePagesBacked = true;
            return makeMappedTable(mapped, mapBytes);
        }
#endif

        (void)::munmap(mapped, mapBytes);
        return TablePtr(nullptr, TableDeleter{AllocationKind::Heap, 0});
    }
#endif

    [[nodiscard]] static inline TablePtr allocateTable(HugePageMode mode, bool& outHugePagesBacked) {
        outHugePagesBacked = false;

        const bool hugePageAllowed = (mode != HugePageMode::Off) && (TABLE_BYTES >= HUGE_PAGE_MIN_BYTES);
        if (hugePageAllowed) {
#if defined(__linux__)
            TablePtr explicitHuge = tryAllocateExplicitHugePages(outHugePagesBacked);
            if (explicitHuge) return explicitHuge;

            TablePtr transparentHuge = tryAllocateTransparentHugePages(outHugePagesBacked);
            if (transparentHuge) return transparentHuge;
#endif
        }

        return allocateHeapTable();
    }

    [[nodiscard]] static inline uint8_t clampDepth(uint8_t depth) noexcept {
        return (depth <= Entry::MAX_DEPTH) ? depth : Entry::MAX_DEPTH;
    }

};

inline void TranspositionTable::prefetch(uint64_t key) noexcept {
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    const Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);
    __builtin_prefetch(bucket, 0, 3);
}

inline bool TranspositionTable::probeMove(uint64_t key, uint16_t& outBestMove) const noexcept {
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    const Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);
    const BucketSeq& bucketSeq = seqData()[bucketIndex];
    EntrySnapshot snapshot[ENTRIES_PER_BUCKET];
    if (!readBucketSnapshot(bucket, bucketSeq, snapshot)) {
        outBestMove = 0;
        return false;
    }

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        const EntrySnapshot& entry = snapshot[i];
        if (entry.key != key) continue;

        const uint8_t flag = Entry::flagFromPayload(entry.payload);
        if (flag == Entry::INVALID) continue;

        outBestMove = Entry::bestMoveFromPayload(entry.payload);
        return outBestMove != 0;
    }
    outBestMove = 0;
    return false;
}

inline bool TranspositionTable::probe(uint64_t key, uint8_t depth,
                                      int32_t alpha, int32_t beta, int32_t& outScore) noexcept {
    const uint8_t neededDepth = clampDepth(depth);
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    const Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);
    const BucketSeq& bucketSeq = seqData()[bucketIndex];
    EntrySnapshot snapshot[ENTRIES_PER_BUCKET];
    if (!readBucketSnapshot(bucket, bucketSeq, snapshot)) {
        return false;
    }

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        const EntrySnapshot& entry = snapshot[i];
        if (entry.key != key) continue;

        const uint8_t flag = Entry::flagFromPayload(entry.payload);
        if (flag == Entry::INVALID) continue;
        if (Entry::depthFromPayload(entry.payload) < neededDepth) continue;

        const int32_t score = Entry::scoreFromPayload(entry.payload);
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
    const BucketSeq& bucketSeq = seqData()[bucketIndex];
    EntrySnapshot snapshot[ENTRIES_PER_BUCKET];
    if (!readBucketSnapshot(bucket, bucketSeq, snapshot)) {
        outBestMove = 0;
        return false;
    }

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        const EntrySnapshot& entry = snapshot[i];
        if (entry.key != key) continue;

        const uint8_t flag = Entry::flagFromPayload(entry.payload);
        if (flag == Entry::INVALID) continue;

        outBestMove = Entry::bestMoveFromPayload(entry.payload);
        if (Entry::depthFromPayload(entry.payload) < neededDepth) return false;

        const int32_t score = Entry::scoreFromPayload(entry.payload);
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
    BucketSeq& bucketSeq = seqData()[bucketIndex];
    const uint32_t lockBase = lockBucket(bucketSeq);

    Entry* replaceEntry = &bucket[0];
    Entry* emptyEntry = nullptr;
    int bestReplaceScore = INT32_MIN;

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        Entry& entry = bucket[i];
        const uint64_t entryKey = loadEntryKeyAtomic(entry);
        const uint64_t entryPayload = loadEntryPayloadAtomic(entry);
        const uint8_t entryFlag = Entry::flagFromPayload(entryPayload);

        if (entryFlag == Entry::INVALID) {
            if (emptyEntry == nullptr) emptyEntry = &entry;
            continue;
        }

        if (entryKey == key) {
            if (storedDepth >= Entry::depthFromPayload(entryPayload) || flag == Entry::EXACT) {
                const uint16_t keepBestMove = Entry::bestMoveFromPayload(entryPayload);
                const uint64_t newPayload = Entry::encodePayload(score, keepBestMove, storedDepth, generation_, flag);
                storeEntryPayloadAtomic(entry, newPayload);
            }
            unlockBucket(bucketSeq, lockBase);
            return;
        }

        const int ageDiff = static_cast<int>(static_cast<uint8_t>(generation_ - Entry::ageFromPayload(entryPayload)));
        const int replaceScore = (ageDiff << 8) - (static_cast<int>(Entry::depthFromPayload(entryPayload)) << 2);
        if (replaceScore > bestReplaceScore) {
            bestReplaceScore = replaceScore;
            replaceEntry = &entry;
        }
    }

    Entry* const target = (emptyEntry != nullptr) ? emptyEntry : replaceEntry;
    const uint64_t newPayload = Entry::encodePayload(score, 0, storedDepth, generation_, flag);
    storeEntryPayloadAtomic(*target, newPayload);
    storeEntryKeyAtomic(*target, key);
    unlockBucket(bucketSeq, lockBase);
}

inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept {
    const uint8_t storedDepth = clampDepth(depth);
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
    Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);
    BucketSeq& bucketSeq = seqData()[bucketIndex];
    const uint32_t lockBase = lockBucket(bucketSeq);

    Entry* replaceEntry = &bucket[0];
    Entry* emptyEntry = nullptr;
    int bestReplaceScore = INT32_MIN;

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        Entry& entry = bucket[i];
        const uint64_t entryKey = loadEntryKeyAtomic(entry);
        const uint64_t entryPayload = loadEntryPayloadAtomic(entry);
        const uint8_t entryFlag = Entry::flagFromPayload(entryPayload);

        if (entryFlag == Entry::INVALID) {
            if (emptyEntry == nullptr) emptyEntry = &entry;
            continue;
        }

        if (entryKey == key) {
            if (storedDepth >= Entry::depthFromPayload(entryPayload) || flag == Entry::EXACT) {
                const uint64_t newPayload = Entry::encodePayload(score, bestMove, storedDepth, generation_, flag);
                storeEntryPayloadAtomic(entry, newPayload);
            }
            unlockBucket(bucketSeq, lockBase);
            return;
        }

        const int ageDiff = static_cast<int>(static_cast<uint8_t>(generation_ - Entry::ageFromPayload(entryPayload)));
        const int replaceScore = (ageDiff << 8) - (static_cast<int>(Entry::depthFromPayload(entryPayload)) << 2);
        if (replaceScore > bestReplaceScore) {
            bestReplaceScore = replaceScore;
            replaceEntry = &entry;
        }
    }

    Entry* const target = (emptyEntry != nullptr) ? emptyEntry : replaceEntry;
    const uint64_t newPayload = Entry::encodePayload(score, bestMove, storedDepth, generation_, flag);
    storeEntryPayloadAtomic(*target, newPayload);
    storeEntryKeyAtomic(*target, key);
    unlockBucket(bucketSeq, lockBase);
}

inline void TranspositionTable::clear() noexcept {
    std::fill_n(data(), TABLE_SIZE, Entry{});
    BucketSeq* bucketSeq = seqData();
    for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
        bucketSeq[i].value.store(0U, std::memory_order_relaxed);
    }
}

inline constexpr TranspositionTable::Entry::Flag
determineFlag(int32_t score, int32_t alphaOrig, int32_t beta) noexcept {
    if (score <= alphaOrig) return TranspositionTable::Entry::UPPERBOUND;
    if (score >= beta) return TranspositionTable::Entry::LOWERBOUND;
    return TranspositionTable::Entry::EXACT;
}

static_assert(determineFlag(100, 50, 200) == TranspositionTable::Entry::EXACT, "determineFlag logic error");
static_assert(determineFlag(40, 50, 200) == TranspositionTable::Entry::UPPERBOUND, "determineFlag logic error");
static_assert(determineFlag(250, 50, 200) == TranspositionTable::Entry::LOWERBOUND, "determineFlag logic error");

