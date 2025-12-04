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

    Coords() noexcept = default;
    
    Coords(uint8_t index) noexcept : Coords() {
        if (index < 64) {
            this->file = index & 7;
            this->rank = index >> 3;
        }
    }

    Coords(uint8_t f, uint8_t r) noexcept : Coords() {
        if (this->isValid(f)) this->file = f;
        if (this->isValid(r)) this->rank = r;
    }

    Coords(const std::string& input) noexcept : Coords() {
        if (input.length() != 2) return;
        if (this->isLetter(std::tolower(input[0]))) file = std::tolower(input[0]) - 'a';
        if (this->isNumber(input[1])) rank = input[1] - '1';
    }

    Coords(const Coords& c) noexcept = default;



    // operator overloads

    bool operator==(const Coords &other) const noexcept { return (this->file == other.file) && (this->rank == other.rank);}
    bool operator!=(const Coords &other) const noexcept { return !(*this == other);}
    
    Coords& operator=(const Coords &other) noexcept {
        if (this != &other) {
            this->file = other.file;
            this->rank = other.rank;
        } return *this;
    }


    // setters / updaters

    bool update(const Coords& other) noexcept{ return this->update(other.file, other.rank); } 
    
    bool update(const uint8_t f, const uint8_t r) noexcept{
        if (f == INVALID_COORDS || r == INVALID_COORDS) return false;
        this->file = f;
        this->rank = r;
        return true;
    }


    
    // utility static methods

    static bool isValid(uint8_t x) noexcept { return (x < 8); }
    static bool isLetter(char c) noexcept { return ((c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H')); }
    static bool isNumber(char c) noexcept { return (c >= '1') && (c <= '8'); }
    static bool isInBounds(const Coords& coords) noexcept { return (isValid(coords.file) && isValid(coords.rank)); }
    
    
    

    // conversion methods

    std::string toString() noexcept {
        std::string result(2, ' ');
        if (isInBounds(*this)) {
            result[0] = static_cast<char>('a' + this->file);
            result[1] = static_cast<char>('1' + this->rank);
        } else {
            result = "??";
        }
        return result;
    }
    
    uint8_t toIndex() const noexcept { return static_cast<uint8_t>(this->rank * 8 + this->file); }
    
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
    