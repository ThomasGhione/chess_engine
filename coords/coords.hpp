#ifndef COORDS
#define COORDS

#include <string>
#include <cstdint>
#include "defines.hpp"

namespace chess {

class Coords {

public:
    uint8_t file;
    uint8_t rank;

    Coords();
    Coords(uint8_t f, uint8_t r);
    Coords(std::string input);

    bool operator==(const Coords &other) const;
    bool operator!=(const Coords &other) const;

    Coords& operator=(const Coords &other) {
        if (this != &other) {
            file = other.file;
            rank = other.rank;
            return *this;
        }
    }

private:
    bool isValid(uint8_t x) const;
};

}

#endif