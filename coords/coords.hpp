#ifndef COORDS
#define COORDS

#include <string>
#include <cstdint>

namespace chess {

class Coords {

public:

    constexpr static uint8_t INVALID_COORDS = 255;

    uint8_t file = INVALID_COORDS; // column
    uint8_t rank = INVALID_COORDS; // row



    // constructors

    constexpr Coords() noexcept = default;
    
    constexpr Coords(uint8_t index) noexcept : Coords() {
        if (index < 64) {
            file = index & 7;
            rank = index >> 3;
        }
    }

    constexpr Coords(uint8_t f, uint8_t r) noexcept : Coords() {
        if (isValid(f)) file = f;
        if (isValid(r)) rank = r;
    }

    Coords(const std::string& input) noexcept : Coords() {
        if (input.length() != 2) return;
        char f = std::tolower(input[0]);
        if (isLetter(f)) file = f - 'a';
        if (isNumber(input[1])) rank = input[1] - '1';
    }

    constexpr Coords(const Coords& c) noexcept = default;



    // operator overloads

    constexpr bool operator==(const Coords &other) const noexcept { return (file == other.file) && (rank == other.rank);}
    constexpr bool operator!=(const Coords &other) const noexcept { return !(*this == other);}
    
    constexpr Coords& operator=(const Coords &other) noexcept {
        file = other.file;
        rank = other.rank;
        return *this;
    }


    // setters / updaters

    constexpr bool update(const Coords& other) noexcept{ return update(other.file, other.rank); } 
    
    constexpr bool update(const uint8_t f, const uint8_t r) noexcept{
        if (!isValid(f) || !isValid(r)) return false;
        file = f;
        rank = r;
        return true;
    }


    
    // utility static methods

    static constexpr bool isValid(uint8_t x) noexcept { return (x < 8); }
    static constexpr bool isLetter(char c) noexcept { return ((c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H')); }
    static constexpr bool isNumber(char c) noexcept { return (c >= '1') && (c <= '8'); }
    static constexpr bool isInBounds(const Coords& coords) noexcept { return (isValid(coords.file) && isValid(coords.rank)); }
    
    
    

    // conversion methods

    std::string toString() const noexcept {
        std::string result(2, ' ');
        if (isInBounds(*this)) {
            result[0] = static_cast<char>('a' + file);
            result[1] = static_cast<char>('1' + rank);
        } else {
            result = "??";
        }
        return result;
    }
    
    constexpr uint8_t toIndex() const noexcept { return static_cast<uint8_t>(rank * 8 + file); }
    
    static std::string toAlgebric(const Coords& c) noexcept {
        char fileChar = static_cast<char>('a' + c.file);
        char rankChar = static_cast<char>('1' + c.rank);
        std::string s;
        s.push_back(fileChar);
        s.push_back(rankChar);
        return s;
    }

}; // class Coords

} // namespace chess
#endif
    