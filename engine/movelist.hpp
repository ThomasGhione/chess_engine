#pragma once

#include <cstdint>
#include <cstring>
#include <utility>  // for std::forward
#include <new>
#include <type_traits>

inline constexpr size_t MAX_MOVES = 218;

template<typename T, size_t MAX_SIZE = MAX_MOVES>
struct MoveList {
    // moveFrom() / copyFrom() take the trivially-copyable fast path and rely on
    // T being trivially destructible (sources left in their old slots are not
    // ~T()'d before the destination is overwritten). Hold the invariant
    // explicitly so future T changes can't silently leak resources.
    static_assert(std::is_trivially_destructible_v<T>,
                  "MoveList requires trivially-destructible T (memcpy fast path).");

    // alignas(T) std::byte[sizeof(T)] replaces std::aligned_storage_t, which is
    // deprecated in C++23. Layout is byte-identical: sizeof == sizeof(T),
    // alignof == alignof(T).
    struct alignas(T) Storage { std::byte bytes[sizeof(T)]; };
    Storage data[MAX_SIZE];
    int size = 0;

    MoveList() noexcept = default;

    ~MoveList() noexcept {
        clear();
    }

    MoveList(const MoveList& other) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        copyFrom(other);
    }

    MoveList& operator=(const MoveList& other) noexcept(
        std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>) {
        if (this != &other) {
            clear();
            copyFrom(other);
        }
        return *this;
    }

    MoveList(MoveList&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        moveFrom(std::move(other));
    }

    MoveList& operator=(MoveList&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>) {
        if (this != &other) {
            clear();
            moveFrom(std::move(other));
        }
        return *this;
    }

    inline void push_back(const T& m) noexcept {
        new (&data[size]) T(m);
        ++size;
    }

    template<typename... Args>
    inline void emplace_back(Args&&... args) noexcept {
        new (&data[size]) T(std::forward<Args>(args)...);
        ++size;
    }

    inline T& operator[](size_t i) noexcept { return *ptr(i); }
    inline const T& operator[](size_t i) const noexcept { return *ptr(i); }

    inline T* begin() noexcept { return ptr(0); }
    inline T* end() noexcept { return ptr(size); }
    inline const T* begin() const noexcept { return ptr(0); }
    inline const T* end() const noexcept { return ptr(size); }

    [[nodiscard]] inline bool is_empty() const noexcept { return size == 0; }

    inline void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (int i = 0; i < size; ++i) {
                ptr(i)->~T();
            }
        }
        size = 0;
    }

private:

    inline T* ptr(size_t i) noexcept { return reinterpret_cast<T*>(&data[i]); }
    inline const T* ptr(size_t i) const noexcept { return reinterpret_cast<const T*>(&data[i]); }

    inline void copyFrom(const MoveList& other) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        size = other.size;
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data, other.data, static_cast<size_t>(size) * sizeof(T));
        } else {
            for (int i = 0; i < size; ++i) {
                new (&data[i]) T(*other.ptr(i));
            }
        }
    }

    inline void moveFrom(MoveList&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        size = other.size;
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data, other.data, static_cast<size_t>(size) * sizeof(T));
        } else {
            for (int i = 0; i < size; ++i) {
                new (&data[i]) T(std::move(*other.ptr(i)));
            }
        }
        other.size = 0;
    }

};
