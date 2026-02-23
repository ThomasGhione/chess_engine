#ifndef COORDS
#define COORDS

#include <string>
#include <cstdint>
#include <cctype>

namespace chess {

struct Coords {

    constexpr static uint8_t INVALID_COORDS = 255;

    // Board convention: a8=0, b8=1, ..., h8=7, a7=8, ..., h1=63

    uint8_t index = INVALID_COORDS;

    // ============== CONSTRUCTORS ==============

    constexpr Coords() noexcept = default;

    constexpr explicit Coords(uint8_t idx) noexcept
        : index(idx < 64 ? idx : INVALID_COORDS) 
    {}

    constexpr Coords(uint8_t f, uint8_t r) noexcept
        : index((f < 8 && r < 8) ? static_cast<uint8_t>(r * 8 + f) : INVALID_COORDS) 
    {}

    // Constructor from algebraic notation string (e.g. "e4", "a8")
    explicit Coords(const std::string& input) noexcept 
        : index(INVALID_COORDS) 
    {
        if (input.length() != 2) [[unlikely]] return;

        const char fileChar = std::tolower(input[0]);
        const char rankChar = input[1];

        if (!isLetter(fileChar) || !isNumber(rankChar)) [[unlikely]] return;

        const uint8_t f = static_cast<uint8_t>(fileChar - 'a');
        const uint8_t r = static_cast<uint8_t>('8' - rankChar);

        this->index = r * 8 + f;
    }

    constexpr Coords(const Coords& other) noexcept = default;

    // ============== ACCESS METHODS ==============

    constexpr uint8_t file() const noexcept { return this->index & 7; }
    constexpr uint8_t rank() const noexcept { return this->index >> 3; }
    constexpr bool isValid() const noexcept { return this->index < 64; }

    // ============== OPERATOR OVERLOADS ==============

    constexpr bool operator==(const Coords& other) const noexcept { return this->index == other.index; }
    constexpr bool operator!=(const Coords& other) const noexcept { return this->index != other.index; }
    constexpr Coords& operator=(const Coords& other) noexcept = default;

    // ============== SETTERS / UPDATERS ==============

    constexpr bool update(const Coords& other) noexcept {
        if (!other.isValid()) [[unlikely]] return false;
        this->index = other.index;
        return true;
    }

    constexpr bool update(uint8_t f, uint8_t r) noexcept {
        if (f >= 8 || r >= 8) [[unlikely]] return false;
        this->index = r * 8 + f;
        return true;
    }

    // Update from index
    constexpr bool update(uint8_t idx) noexcept {
        if (idx >= 64) [[unlikely]] return false;
        this->index = idx;
        return true;
    }

    // ============== UTILITY STATIC METHODS ==============

    static constexpr bool isValid(uint8_t x) noexcept { return x < 8; }
    static constexpr bool isLetter(char c) noexcept {
        return (c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H');
    }
    static constexpr bool isNumber(char c) noexcept {
        return c >= '1' && c <= '8';
    }
    static constexpr bool isInBounds(const Coords& coords) noexcept {
        return coords.isValid();
    }

    // ============== CONVERSION METHODS ==============

    // Convert to algebraic notation string (e.g. "e4")
    std::string toString() const noexcept {
        if (!this->isValid()) [[unlikely]] return "??";

        std::string result(2, ' ');
        const uint8_t f = this->file();
        const uint8_t r = this->rank();

        // file: 0-7 -> 'a'-'h'
        result[0] = static_cast<char>('a' + f);
        // rank: 0-7 -> '8'-'1' (a8=0, h1=63 convention)
        result[1] = static_cast<char>('8' - r);

        return result;
    }

    static std::string toAlgebric(const Coords& c) noexcept { return c.toString(); }

}; // class Coords

} // namespace chess
#endif
