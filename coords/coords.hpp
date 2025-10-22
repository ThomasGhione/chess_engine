#ifndef COORDS
#define COORDS

#include <string>
#include <cstdint>

namespace chess {

class Coords {

public:
    uint8_t file; // column
    uint8_t rank; // row

    constexpr static uint8_t INVALID_COORDS = 255;

    Coords();
    Coords(uint8_t f, uint8_t r);
    Coords(const std::string& input);
    Coords(const Coords& c) = default;

    bool operator==(const Coords &other) const;
    bool operator!=(const Coords &other) const;
    Coords& operator=(const Coords &other);

    bool update(const Coords& other);
    bool update(const uint8_t f, const uint8_t r);

    static bool isValid(uint8_t x);
    static bool isLetter(char c);
    static bool isNumber(char c);
    static bool isInBounds(const Coords& coords);
};

}

#endif
