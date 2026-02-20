#ifndef COORDS
#define COORDS

#include <string>
#include <cstdint>
#include <cctype>

namespace chess {
class Coords {
public:
    constexpr static uint8_t INVALID_COORDS = 255;

    // Storage efficiente: solo 1 byte invece di 2
    // Convenzione Board: a8=0, b8=1, ..., h8=7, a7=8, ..., h1=63

    uint8_t index = INVALID_COORDS;

    // ============== COSTRUTTORI ==============

    // Default constructor: invalid coordinates
    constexpr Coords() noexcept = default;

    // Constructor from index (0-63)
    // Uses inline validation for efficiency
    constexpr explicit Coords(uint8_t idx) noexcept
        : index(idx < 64 ? idx : INVALID_COORDS) {}

    // Constructor from file and rank (0-7 each)
    // Formula: index = rank * 8 + file
    constexpr Coords(uint8_t f, uint8_t r) noexcept
        : index((f < 8 && r < 8) ? static_cast<uint8_t>(r * 8 + f) : INVALID_COORDS) {}

    // Constructor from algebraic notation string (e.g. "e4", "a8")
    // NOT constexpr because it uses std::tolower, which is not constexpr
    explicit Coords(const std::string& input) noexcept : index(INVALID_COORDS) {
        if (input.length() != 2) return;

        char fileChar = std::tolower(input[0]);
        char rankChar = input[1];

        if (!isLetter(fileChar) || !isNumber(rankChar)) return;

        // Convert algebraic notation to internal index
        // file: 'a'=0, 'b'=1, ..., 'h'=7
        uint8_t f = static_cast<uint8_t>(fileChar - 'a');

        // rank: '8'=0, '7'=1, ..., '1'=7 (a8=0, h1=63 convention)
        // Formula: internal_rank = '8' - rankChar
        uint8_t r = static_cast<uint8_t>('8' - rankChar);

        this->index = r * 8 + f;
    }

    // Copy constructor (default is fine)
    constexpr Coords(const Coords& other) noexcept = default;

    // ============== METODI DI ACCESSO PUBBLICI ==============

    // Returns the file (0-7: a-h)
    // Use bit-mask for optimal performance (equivalent to index % 8)
    constexpr uint8_t file() const noexcept {
        return this->index & 7;
    }

    // Returns the rank (0-7: where 0=rank8, 7=rank1 in Board convention)
    // Use bit-shift for optimal performance (equivalent to index / 8)
    constexpr uint8_t rank() const noexcept {
        return this->index >> 3;
    }

    // Checks whether coordinates are valid
    constexpr bool isValid() const noexcept {
        return this->index < 64;
    }

    // ============== OPERATOR OVERLOADS ==============

    constexpr bool operator==(const Coords& other) const noexcept {
        return this->index == other.index;
    }

    constexpr bool operator!=(const Coords& other) const noexcept {
        return this->index != other.index;
    }

    constexpr Coords& operator=(const Coords& other) noexcept = default;

    // ============== SETTERS / UPDATERS ==============

    // Update from another Coords object
    constexpr bool update(const Coords& other) noexcept {
        if (!other.isValid()) return false;
        this->index = other.index;
        return true;
    }

    // Update from file and rank
    constexpr bool update(uint8_t f, uint8_t r) noexcept {
        if (f >= 8 || r >= 8) return false;
        this->index = r * 8 + f;
        return true;
    }

    // Update from index
    constexpr bool update(uint8_t idx) noexcept {
        if (idx >= 64) return false;
        this->index = idx;
        return true;
    }

    // ============== UTILITY STATIC METHODS ==============

    // Checks whether a value is in range 0-7
    static constexpr bool isValid(uint8_t x) noexcept {
        return x < 8;
    }

    // Checks whether a character is a valid file letter
    static constexpr bool isLetter(char c) noexcept {
        return (c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H');
    }

    // Checks whether a character is a valid rank number
    static constexpr bool isNumber(char c) noexcept {
        return c >= '1' && c <= '8';
    }

    // Checks whether a Coords object is in bounds
    static constexpr bool isInBounds(const Coords& coords) noexcept {
        return coords.isValid();
    }

    // ============== CONVERSION METHODS ==============

    // Convert to algebraic notation string (e.g. "e4")
    std::string toString() const noexcept {
        if (!this->isValid()) {
            return "??";
        }

        std::string result(2, ' ');
        uint8_t f = this->file();
        uint8_t r = this->rank();

        // file: 0-7 -> 'a'-'h'
        result[0] = static_cast<char>('a' + f);

        // rank: 0-7 -> '8'-'1' (a8=0, h1=63 convention)
        result[1] = static_cast<char>('8' - r);

        return result;
    }

    // Convert to index (deprecated, use index() instead)
    // Kept for compatibility
    constexpr uint8_t toIndex() const noexcept {
        return this->index;
    }

    // Static method for conversion to algebraic notation
    static std::string toAlgebric(const Coords& c) noexcept {
        return c.toString();
    }

}; // class Coords
} // namespace chess
#endif
