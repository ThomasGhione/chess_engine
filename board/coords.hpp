#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <cctype>

namespace chess {

inline constexpr uint8_t file(uint8_t sq) noexcept { return sq & 7; }
inline constexpr uint8_t rank(uint8_t sq) noexcept { return sq >> 3; }

struct Coords {

    constexpr static uint8_t INVALID_COORDS = 255;

    // Board convention: a8=0, b8=1, ..., h8=7, a7=8, ..., h1=63

    uint8_t index = INVALID_COORDS;

    // ============== CONSTRUCTORS ==============

    constexpr Coords() noexcept = default;

    //FIXME Mettere di base a INVALID_COORDS e poi effettuare i conti dentro
    constexpr explicit Coords(uint8_t idx) noexcept
        : index(idx < 64 ? idx : INVALID_COORDS) 
    {}

    constexpr Coords(uint8_t f, uint8_t r) noexcept
        : index((f < 8 && r < 8) ? (r * 8 + f) : INVALID_COORDS) 
    {}

    // Constructor from algebraic notation string (e.g. "e4", "a8")
    explicit Coords(std::string_view input) noexcept
        : index(INVALID_COORDS)
    {
        if (input.length() != 2) [[unlikely]] return;

        const char fileChar = static_cast<char>(std::tolower(static_cast<unsigned char>(input[0])));
        const char rankChar = input[1];

        if (fileChar < 'a' || fileChar > 'h' || rankChar < '1' || rankChar > '8') [[unlikely]] return;

        this->index = static_cast<uint8_t>(('8' - rankChar) * 8 + (fileChar - 'a'));
    }

    // ============== ACCESS METHODS ==============

    //FIXME Eliminare costati magiche
    constexpr uint8_t file() const noexcept { return this->index & 7; }
    constexpr uint8_t rank() const noexcept { return this->index >> 3; }
    constexpr bool isValid() const noexcept { return this->index < 64; }

    // ============== OPERATOR OVERLOADS ==============

    constexpr bool operator==(const Coords& other) const noexcept { return this->index == other.index; }
    constexpr bool operator!=(const Coords& other) const noexcept { return this->index != other.index; }

    // ============== SETTERS / UPDATERS ==============

    // ============== CONVERSION METHODS ==============

    // Convert to algebraic notation string (e.g. "e4")
    std::string toString() const noexcept {
        if (!this->isValid()) [[unlikely]] return "??";
        return {static_cast<char>('a' + this->file()), static_cast<char>('8' - this->rank())};
    }


}; // class Coords

} // namespace chess
