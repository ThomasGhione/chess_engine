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
#include "../ascii_utils.hpp"

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

        struct DecodedMove {
            uint8_t from = 0;
            uint8_t to = 0;
            char promo = '\0';
        };

        static constexpr uint8_t promoCodeFromChar(char promo) noexcept {
            switch (promo) {
                case 'q': case 'Q': return 1;
                case 'r': case 'R': return 2;
                case 'b': case 'B': return 3;
                case 'n': case 'N': return 4;
                default: return 0;
            }
        }

        static constexpr char promoCharFromCode(uint8_t promoCode) noexcept {
            switch (promoCode) {
                case 1: return 'q';
                case 2: return 'r';
                case 3: return 'b';
                case 4: return 'n';
                default: return '\0';
            }
        }

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

        static constexpr uint16_t encodeMove(uint8_t from, uint8_t to, char promo) noexcept {
            return (static_cast<uint16_t>(from) & 0x3F)
                 | ((static_cast<uint16_t>(to) & 0x3F) << 6)
                 | ((static_cast<uint16_t>(promoCodeFromChar(promo)) & 0xF) << 12);
        }

        static constexpr DecodedMove decodeMove(uint16_t encoded) noexcept {
            return DecodedMove{
                static_cast<uint8_t>(encoded & 0x3F),
                static_cast<uint8_t>((encoded >> 6) & 0x3F),
                promoCharFromCode(static_cast<uint8_t>((encoded >> 12) & 0xF))
            };
        }
    };

    static constexpr size_t BUCKET_COUNT = 1u << 20; // 1M buckets = 4M entries = 64 MiB, tests/perf tuned.
    static constexpr size_t ENTRIES_PER_BUCKET = 4;
    static constexpr size_t TABLE_SIZE = BUCKET_COUNT * ENTRIES_PER_BUCKET;
    static constexpr size_t TABLE_BYTES = sizeof(Entry) * TABLE_SIZE;
    static constexpr size_t HUGE_PAGE_MIN_BYTES = 32u * 1024u * 1024u; // 32 MiB, common huge page size on x86-64 Linux.

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
    inline bool probeSE(uint64_t key, uint8_t minDepth, int32_t& outScore, uint8_t& outFlag) const noexcept;
    inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept;
    inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept;

    inline void incrementGeneration() noexcept { ++generation_; }
    inline void clear() noexcept;

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
        size_t mappedBytes = 0;

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

    [[nodiscard]] static inline std::atomic_ref<uint64_t> atomicWord(const uint64_t& word) noexcept {
        return std::atomic_ref<uint64_t>(const_cast<uint64_t&>(word));
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

            for (size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
                snapshot[i].key = atomicWord(bucket[i].key).load(std::memory_order_relaxed);
                snapshot[i].payload = atomicWord(bucket[i].payload).load(std::memory_order_relaxed);
            }

            const uint32_t seqEnd = bucketSeq.value.load(std::memory_order_acquire);
            if (seqStart == seqEnd && (seqEnd & 1U) == 0U) {
                return true;
            }
        }
        return false;
    }

    // Shared probe scaffolding: snapshot the bucket and return the first valid
    // entry whose key matches (the only entry any probe ever inspects, since
    // they all stop at the first key match). Returns false if the lock-free
    // snapshot read failed or no matching valid entry exists.
    [[nodiscard]] __attribute__((always_inline)) inline bool findEntrySnapshot(
        uint64_t key, EntrySnapshot& out) const noexcept {
        const size_t bucketIndex = static_cast<size_t>(key) & (BUCKET_COUNT - 1);
        const Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);

        const BucketSeq& bucketSeq = seqData()[bucketIndex];
        EntrySnapshot snapshot[ENTRIES_PER_BUCKET];
        if (!readBucketSnapshot(bucket, bucketSeq, snapshot)) return false;
        for (size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            const EntrySnapshot& entry = snapshot[i];
            if (entry.key != key) continue;
            if (Entry::flagFromPayload(entry.payload) == Entry::INVALID) continue;
            out = entry;
            return true;
        }
        return false;
    }

    [[nodiscard]] static inline HugePageMode hugePageModeFromEnv() noexcept {
        const char* envValue = std::getenv("CHESS_TT_HUGEPAGE");
        if (envValue == nullptr || *envValue == '\0') return HugePageMode::Auto;

        const std::string_view value(envValue);
        if (ascii::iequals(value, "on") || ascii::iequals(value, "1") || ascii::iequals(value, "true") || ascii::iequals(value, "force")) {
            return HugePageMode::On;
        }
        if (ascii::iequals(value, "off") || ascii::iequals(value, "0") || ascii::iequals(value, "false")) {
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

    [[nodiscard]] static constexpr size_t alignUp(size_t value, size_t alignment) noexcept {
        return (value + alignment - 1u) & ~(alignment - 1u);
    }

    [[nodiscard]] static inline TablePtr allocateHeapTable() {
        void* raw = ::operator new(sizeof(TableStorage), std::align_val_t(alignof(TableStorage)));
        auto* table = ::new (raw) TableStorage();
        return TablePtr(table, TableDeleter{AllocationKind::Heap, 0});
    }

#if defined(__linux__)
    [[nodiscard]] static inline TablePtr makeMappedTable(void* mappedMemory, size_t mappedBytes) {
        auto* table = ::new (mappedMemory) TableStorage();
        return TablePtr(table, TableDeleter{AllocationKind::MMap, mappedBytes});
    }

    [[nodiscard]] static inline TablePtr tryAllocateExplicitHugePages(bool& outHugePagesBacked) {
        constexpr size_t HugePageBytes = 2u * 1024u * 1024u;
        const size_t mapBytes = alignUp(sizeof(TableStorage), HugePageBytes);
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
        const size_t mapBytes = sizeof(TableStorage);
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

    [[nodiscard]] static constexpr uint8_t clampDepth(uint8_t depth) noexcept {
        return (depth <= Entry::MAX_DEPTH) ? depth : Entry::MAX_DEPTH;
    }

    inline void storeImpl(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove, bool replaceBestMove) noexcept;

};

inline void TranspositionTable::prefetch(uint64_t key) noexcept {
    const size_t bucketIndex = static_cast<size_t>(key) & (BUCKET_COUNT - 1);
    __builtin_prefetch(data() + (bucketIndex * ENTRIES_PER_BUCKET), 0, 3);
    __builtin_prefetch(seqData() + bucketIndex, 0, 3);
}

static_assert(TranspositionTable::Entry::encodeMove(12, 28, 'q') == TranspositionTable::Entry::encodeMove(12, 28, 'Q'), "promotion encoding should be case-insensitive");
static_assert(TranspositionTable::Entry::decodeMove(TranspositionTable::Entry::encodeMove(12, 28, 'n')).from == 12, "move decode from mismatch");
static_assert(TranspositionTable::Entry::decodeMove(TranspositionTable::Entry::encodeMove(12, 28, 'n')).to == 28, "move decode to mismatch");
static_assert(TranspositionTable::Entry::decodeMove(TranspositionTable::Entry::encodeMove(12, 28, 'n')).promo == 'n', "move decode promotion mismatch");

inline bool TranspositionTable::probeMove(uint64_t key, uint16_t& outBestMove) const noexcept {
    EntrySnapshot entry;
    if (!findEntrySnapshot(key, entry)) {
        outBestMove = 0;
        return false;
    }
    outBestMove = Entry::bestMoveFromPayload(entry.payload);
    return outBestMove != 0;
}

inline bool TranspositionTable::probe(uint64_t key, uint8_t depth,
                                      int32_t alpha, int32_t beta, int32_t& outScore) noexcept {
    EntrySnapshot entry;
    if (!findEntrySnapshot(key, entry)) return false;
    if (Entry::depthFromPayload(entry.payload) < clampDepth(depth)) return false;

    const uint8_t flag = Entry::flagFromPayload(entry.payload);
    const int32_t score = Entry::scoreFromPayload(entry.payload);
    if (flag == Entry::EXACT
        || (flag == Entry::LOWERBOUND && score >= beta)
        || (flag == Entry::UPPERBOUND && score <= alpha)) {
        outScore = score;
        return true;
    }
    return false;
}

inline bool TranspositionTable::probeSE(uint64_t key, uint8_t minDepth, int32_t& outScore, uint8_t& outFlag) const noexcept {
    EntrySnapshot entry;
    if (!findEntrySnapshot(key, entry)) return false;
    if (Entry::depthFromPayload(entry.payload) < minDepth) return false;
    outScore = Entry::scoreFromPayload(entry.payload);
    outFlag  = Entry::flagFromPayload(entry.payload);
    return true;
}

inline void TranspositionTable::storeImpl(
    uint64_t key,
    uint8_t depth,
    int32_t score,
    uint8_t flag,
    uint16_t bestMove,
    bool replaceBestMove) noexcept {
    const uint8_t storedDepth = clampDepth(depth);
    const size_t bucketIndex = static_cast<size_t>(key) & (BUCKET_COUNT - 1);
    Entry* bucket = data() + (bucketIndex * ENTRIES_PER_BUCKET);

    BucketSeq& bucketSeq = seqData()[bucketIndex];
    const uint32_t lockBase = lockBucket(bucketSeq);

    Entry* replaceEntry = &bucket[0];
    Entry* emptyEntry = nullptr;
    int bestReplaceScore = INT32_MIN;

    for (size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        Entry& entry = bucket[i];
        const uint64_t entryKey = atomicWord(entry.key).load(std::memory_order_relaxed);
        const uint64_t entryPayload = atomicWord(entry.payload).load(std::memory_order_relaxed);
        const uint8_t entryFlag = Entry::flagFromPayload(entryPayload);

        if (entryFlag == Entry::INVALID) {
            if (emptyEntry == nullptr) emptyEntry = &entry;
            continue;
        }

        if (entryKey == key) {
            if (storedDepth >= Entry::depthFromPayload(entryPayload) || flag == Entry::EXACT) {
                const uint16_t moveToStore = replaceBestMove ? bestMove : Entry::bestMoveFromPayload(entryPayload);
                const uint64_t newPayload = Entry::encodePayload(score, moveToStore, storedDepth, generation_, flag);
                atomicWord(entry.payload).store(newPayload, std::memory_order_relaxed);
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
    atomicWord(target->payload).store(newPayload, std::memory_order_relaxed);
    atomicWord(target->key).store(key, std::memory_order_relaxed);
    unlockBucket(bucketSeq, lockBase);
}

inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept {
    storeImpl(key, depth, score, flag, 0, false);
}

inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag, uint16_t bestMove) noexcept {
    storeImpl(key, depth, score, flag, bestMove, true);
}

inline void TranspositionTable::clear() noexcept {
    std::fill_n(data(), TABLE_SIZE, Entry{});
    BucketSeq* bucketSeq = seqData();
    for (size_t i = 0; i < BUCKET_COUNT; ++i) {
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
