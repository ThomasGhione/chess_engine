#pragma once

#include <cstddef>
#include <cstring>
#include <utility>

#include "../board/board.hpp"

inline constexpr size_t MAX_MOVES = 218;

struct MoveList {
    struct alignas(chess::Move) Storage { std::byte bytes[sizeof(chess::Move)]; };
    Storage data[MAX_MOVES];
    int size = 0;

    MoveList() noexcept = default;
    ~MoveList() noexcept = default;

    MoveList(const MoveList& other) noexcept { copyFrom(other); }
    MoveList& operator=(const MoveList& other) noexcept {
        if (this != &other) copyFrom(other);
        return *this;
    }

    MoveList(MoveList&& other) noexcept { moveFrom(other); }
    MoveList& operator=(MoveList&& other) noexcept {
        if (this != &other) moveFrom(other);
        return *this;
    }

    inline void push_back(const chess::Move& m) noexcept {
        new (&data[size]) chess::Move(m);
        ++size;
    }

    template<typename... Args>
    inline void emplace_back(Args&&... args) noexcept {
        new (&data[size]) chess::Move(std::forward<Args>(args)...);
        ++size;
    }

    inline chess::Move& operator[](size_t i) noexcept { return *ptr(i); }
    inline const chess::Move& operator[](size_t i) const noexcept { return *ptr(i); }

    inline chess::Move* begin() noexcept { return ptr(0); }
    inline chess::Move* end() noexcept { return ptr(size); }
    inline const chess::Move* begin() const noexcept { return ptr(0); }
    inline const chess::Move* end() const noexcept { return ptr(size); }

    [[nodiscard]] inline bool is_empty() const noexcept { return size == 0; }

    inline void rotate(size_t index) noexcept {
        chess::Move temp = (*this)[index];
        for (size_t i = index; i > 0; --i)
            (*this)[i] = (*this)[i - 1];
        (*this)[0] = temp;
    }

private:
    inline chess::Move* ptr(size_t i) noexcept {
        return reinterpret_cast<chess::Move*>(&data[i]);
    }
    inline const chess::Move* ptr(size_t i) const noexcept {
        return reinterpret_cast<const chess::Move*>(&data[i]);
    }

    inline void copyFrom(const MoveList& other) noexcept {
        size = other.size;
        std::memcpy(data, other.data, static_cast<size_t>(size) * sizeof(chess::Move));
    }

    inline void moveFrom(MoveList& other) noexcept {
        size = other.size;
        std::memcpy(data, other.data, static_cast<size_t>(size) * sizeof(chess::Move));
        other.size = 0;
    }
};
