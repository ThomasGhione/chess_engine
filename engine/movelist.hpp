#ifndef ENGINE_MOVELIST_HPP
#define ENGINE_MOVELIST_HPP

#include "engine.hpp"
#include <utility>  // per std::forward
#include <concepts> // per concepts C++20
#include <algorithm> // per std::partial_sort

// Concept: T deve avere un membro .score di tipo int32_t
template<typename T>
concept HasScore = requires(T a, T b) {
    { a.score } -> std::convertible_to<int32_t>;
    { b.score } -> std::convertible_to<int32_t>;
};

template<typename T, size_t MAX_MOVES = 256>
struct MoveList {
    T data[MAX_MOVES];
    int size = 0;

    // ---------------------------------
    // Insertion
    // ---------------------------------
    inline void push_back(const T& m) noexcept {
        data[size++] = m;
    }

    template<typename... Args>
    inline void emplace_back(Args&&... args) noexcept {
        data[size++] = T(std::forward<Args>(args)...);
    }

    // ---------------------------------
    // Access
    // ---------------------------------
    inline T& operator[](size_t i) noexcept { return data[i]; }
    inline const T& operator[](size_t i) const noexcept { return data[i]; }

    // ---------------------------------
    // Iterators
    // ---------------------------------
    inline T* begin() noexcept { return data; }
    inline T* end() noexcept { return data + size; }
    inline const T* begin() const noexcept { return data; }
    inline const T* end() const noexcept { return data + size; }

    // ---------------------------------
    // Utilities
    // ---------------------------------
    [[nodiscard]] inline bool is_empty() const noexcept { return size == 0; }
    // [[nodiscard]] inline uint16_t count() const noexcept { return size; }


    // ---------------------------------
    // Sorting (only available if T has .score member)
    // Insertion sort optimized for small/partially sorted arrays
    // ---------------------------------

    // Full insertion sort - O(n²) but cache-friendly and fast for small n
    inline void sort() noexcept requires HasScore<T> {
        if (size <= 1) return; // nothing to sort
        
        for (int i = 1; i < size; ++i) {
            T key = data[i];
            int j = i - 1;

            // ordine decrescente (highest score first)
            // Tie-breaker: if scores are equal, sort by move index (from, to) for determinism
            while (j >= 0 && (data[j].score < key.score 
                // used for tie breaking to ensure deterministic ordering
                /* 
                || (data[j].score == key.score && 
                              (data[j].move.from.index > key.move.from.index ||
                               (data[j].move.from.index == key.move.from.index && 
                                data[j].move.to.index > key.move.to.index)))
                            */
                            )) {
                data[j + 1] = data[j];
                --j;
            }
            data[j + 1] = key;
        }
    }

    // Partial insertion sort - sorts only first LIMIT elements
    // Useful for move ordering where only top moves matter
    template<int LIMIT = 14>
    inline void partial_sort() noexcept requires HasScore<T> {
        const int n = (size < LIMIT) ? size : LIMIT;

        for (int i = 1; i < n; ++i) {
            T key = data[i];
            int j = i - 1;
            
            // ordine decrescente (highest score first)
            while (j >= 0 && data[j].score < key.score) {
                data[j + 1] = data[j];
                --j;
            }
            data[j + 1] = key;
        }
    }

};

#endif // ENGINE_MOVELIST_HPP
