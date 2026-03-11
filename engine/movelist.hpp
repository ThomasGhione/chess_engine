#ifndef ENGINE_MOVELIST_HPP
#define ENGINE_MOVELIST_HPP

#include <cstdint>
#include <utility>  // for std::forward
#include <concepts>
#include <algorithm> // for std::partial_sort
#include <new>
#include <type_traits>

// Concept: T must expose a .score member convertible to int32_t
template<typename T>
concept HasScore = requires(T a, T b) {
    { a.score } -> std::convertible_to<int32_t>;
    { b.score } -> std::convertible_to<int32_t>;
};

inline constexpr size_t MAX_MOVES = 218;

template<typename T, size_t MAX_SIZE = MAX_MOVES>
struct MoveList {
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
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
    inline T* end() noexcept { return ptr(static_cast<size_t>(size)); }
    inline const T* begin() const noexcept { return ptr(0); }
    inline const T* end() const noexcept { return ptr(static_cast<size_t>(size)); }

    [[nodiscard]] inline bool is_empty() const noexcept { return size == 0; }

    inline void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (int i = 0; i < size; ++i) {
                ptr(static_cast<size_t>(i))->~T();
            }
        }
        size = 0;
    }

    // ---------------------------------
    // Sorting (only available if T has .score member)
    // Insertion sort optimized for small/partially sorted arrays
    // ---------------------------------

    // Full insertion sort - O(n^2) but cache-friendly and fast for small n
    inline void sort() noexcept requires HasScore<T> {
        if (size <= 1) return; // nothing to sort
        
        for (int i = 1; i < size; ++i) {
            T key = (*this)[static_cast<size_t>(i)];
            int j = i - 1;

            while (j >= 0 && ((*this)[static_cast<size_t>(j)].score < key.score )) {
                (*this)[static_cast<size_t>(j + 1)] = (*this)[static_cast<size_t>(j)];
                --j;
            }
            (*this)[static_cast<size_t>(j + 1)] = key;
        }
    }

    // Partial insertion sort - sorts only first LIMIT elements
    // Useful for move ordering where only top moves matter
    template<int LIMIT = 14>
    inline void partial_sort() noexcept requires HasScore<T> {
        const int n = (size < LIMIT) ? size : LIMIT;

        for (int i = 1; i < n; ++i) {
            T key = (*this)[static_cast<size_t>(i)];
            int j = i - 1;
            
            // Descending order (highest score first)
            while (j >= 0 && (*this)[static_cast<size_t>(j)].score < key.score) {
                (*this)[static_cast<size_t>(j + 1)] = (*this)[static_cast<size_t>(j)];
                --j;
            }
            (*this)[static_cast<size_t>(j + 1)] = key;
        }
    }


private:

    inline T* ptr(size_t i) noexcept { return reinterpret_cast<T*>(&data[i]); }
    inline const T* ptr(size_t i) const noexcept { return reinterpret_cast<const T*>(&data[i]); }

    inline void copyFrom(const MoveList& other) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        size = 0;
        for (int i = 0; i < other.size; ++i) {
            new (&data[i]) T(*other.ptr(static_cast<size_t>(i)));
            ++size;
        }
    }

    inline void moveFrom(MoveList&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        size = 0;
        for (int i = 0; i < other.size; ++i) {
            new (&data[i]) T(std::move(*other.ptr(static_cast<size_t>(i))));
            ++size;
        }
        other.clear();
    }

};

#endif // ENGINE_MOVELIST_HPP
