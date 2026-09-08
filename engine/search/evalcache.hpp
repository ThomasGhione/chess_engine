#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace engine {

// Shared cache of raw NNUE static evals, keyed by Zobrist hash.
//
// The transposition table already carries a static eval, but only for nodes
// that store an entry. Nodes that exit through NMP, RFP or ProbCut compute an
// eval and return without storing anything, and none of them have a TT entry to
// merge into, so that work is thrown away. This table keeps it.
//
// A hit returns exactly what Evaluator::evaluate would have returned for the
// same position, so the search tree is unchanged and only the time differs.
//
// One 64-bit word per slot, direct-mapped: the upper 48 bits hold the key, the
// lower 16 the eval. Reads and writes are single relaxed atomic words, so a
// racing writer can only ever make a reader miss, never hand it a torn value.
// The 48-bit tag makes a false hit a 2^-48 event per probe.
class EvalCache {
public:
    static constexpr size_t DEFAULT_SLOTS = 1u << 21; // 16 MiB shared

    EvalCache() = default;
    ~EvalCache() { release(); }
    EvalCache(const EvalCache&) = delete;
    EvalCache& operator=(const EvalCache&) = delete;

    bool resize(size_t slots) noexcept {
        release();
        if (slots == 0 || (slots & (slots - 1)) != 0) return false;
        void* mem = std::aligned_alloc(64, slots * sizeof(uint64_t));
        if (mem == nullptr) return false;
        slots_ = slots;
        mask_ = slots - 1;
        table_ = static_cast<uint64_t*>(mem);
        clear();
        return true;
    }

    void clear() noexcept {
        for (size_t i = 0; i < slots_; ++i) {
            word(i).store(EMPTY, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] bool probe(uint64_t key, int32_t& outEval) const noexcept {
        if (table_ == nullptr) return false;
        const uint64_t slot = word(index(key)).load(std::memory_order_relaxed);
        if ((slot >> EVAL_BITS) != tag(key)) return false;
        outEval = static_cast<int16_t>(static_cast<uint16_t>(slot & EVAL_MASK));
        return true;
    }

    void store(uint64_t key, int32_t eval) noexcept {
        if (table_ == nullptr) return;
        const uint64_t packed = (tag(key) << EVAL_BITS)
                              | static_cast<uint16_t>(static_cast<int16_t>(eval));
        word(index(key)).store(packed, std::memory_order_relaxed);
    }

private:
    static constexpr int      EVAL_BITS = 16;
    static constexpr uint64_t EVAL_MASK = 0xFFFFULL;
    // Tag 0 with eval 0 is the empty pattern; a real position hashing to tag 0
    // simply never caches, which costs one forward.
    static constexpr uint64_t EMPTY = 0ULL;

    [[nodiscard]] size_t index(uint64_t key) const noexcept {
        return static_cast<size_t>(key) & mask_;
    }
    [[nodiscard]] static uint64_t tag(uint64_t key) noexcept {
        return key >> EVAL_BITS;
    }
    [[nodiscard]] std::atomic_ref<uint64_t> word(size_t i) const noexcept {
        return std::atomic_ref<uint64_t>(table_[i]);
    }

    void release() noexcept {
        std::free(table_);
        table_ = nullptr;
        slots_ = 0;
        mask_ = 0;
    }

    uint64_t* table_ = nullptr;
    size_t    slots_ = 0;
    size_t    mask_  = 0;
};

} // namespace engine
