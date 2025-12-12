#ifndef ENGINE_MOVELIST_HPP
#define ENGINE_MOVELIST_HPP

#include "engine.hpp"
#include <utility> // per std::forward

template<typename T, size_t MAX_MOVES = 256>
struct MoveList {
    T data[MAX_MOVES];
    uint16_t size = 0;

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

    inline T& back() noexcept { return data[size - 1]; }
    inline const T& back() const noexcept { return data[size - 1]; }

    inline T& front() noexcept { return data[0]; }
    inline const T& front() const noexcept { return data[0]; }

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
    inline void clear() noexcept { size = 0; }
    [[nodiscard]] inline bool is_empty() const noexcept { return size == 0; }
    // [[nodiscard]] inline uint16_t count() const noexcept { return size; }
};

#endif // ENGINE_MOVELIST_HPP
