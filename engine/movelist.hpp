#pragma once

#include <cstddef>
#include <cstring>
#include <utility>

#include "../board/board.hpp"

inline constexpr size_t MAX_MOVES = 218;

struct MoveList {
    struct alignas(chess::Board::Move) Storage { std::byte bytes[sizeof(chess::Board::Move)]; };
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

    inline void push_back(const chess::Board::Move& m) noexcept {
        new (&data[size]) chess::Board::Move(m);
        ++size;
    }

    template<typename... Args>
    inline void emplace_back(Args&&... args) noexcept {
        new (&data[size]) chess::Board::Move(std::forward<Args>(args)...);
        ++size;
    }

    inline chess::Board::Move& operator[](size_t i) noexcept { return *ptr(i); }
    inline const chess::Board::Move& operator[](size_t i) const noexcept { return *ptr(i); }

    inline chess::Board::Move* begin() noexcept { return ptr(0); }
    inline chess::Board::Move* end() noexcept { return ptr(size); }
    inline const chess::Board::Move* begin() const noexcept { return ptr(0); }
    inline const chess::Board::Move* end() const noexcept { return ptr(size); }

    [[nodiscard]] inline bool is_empty() const noexcept { return size == 0; }

    inline void rotate(size_t index) noexcept {
        chess::Board::Move temp = (*this)[index];
        for (size_t i = index; i > 0; --i)
            (*this)[i] = (*this)[i - 1];
        (*this)[0] = temp;
    }

private:
    inline chess::Board::Move* ptr(size_t i) noexcept {
        return reinterpret_cast<chess::Board::Move*>(&data[i]);
    }
    inline const chess::Board::Move* ptr(size_t i) const noexcept {
        return reinterpret_cast<const chess::Board::Move*>(&data[i]);
    }

    inline void copyFrom(const MoveList& other) noexcept {
        size = other.size;
        std::memcpy(data, other.data, static_cast<size_t>(size) * sizeof(chess::Board::Move));
    }

    inline void moveFrom(MoveList& other) noexcept {
        size = other.size;
        std::memcpy(data, other.data, static_cast<size_t>(size) * sizeof(chess::Board::Move));
        other.size = 0;
    }
};
