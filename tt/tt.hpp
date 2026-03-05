#ifndef TT_TRANSPOSITION_TABLE_HPP
#define TT_TRANSPOSITION_TABLE_HPP

#include <algorithm>
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

namespace tt {

class TranspositionTable {
public:
    enum class HugePageMode : uint8_t {
        Auto = 0,
        On,
        Off
    };

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
    struct alignas(64) TableStorage {
        std::array<Entry, TABLE_SIZE> entries{};
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

    for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
        const Entry& entry = bucket[i];
        if (entry.key != key) continue;
        if (entry.flag() == Entry::INVALID) continue;

        outBestMove = entry.bestMove;
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

inline void TranspositionTable::clear() noexcept {
    std::fill_n(data(), TABLE_SIZE, Entry{});
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

} // namespace tt

#endif // TT_TRANSPOSITION_TABLE_HPP
