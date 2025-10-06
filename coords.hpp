#ifndef COORDS
#define COORDS

#include <cstdint>
#include "defines.hpp"

namespace chess {

struct coords {
    uint8_t file;
    uint8_t rank;

    coords() 
        : file(EMPTY)
        , rank(EMPTY) 
    {}
    
    coords(uint8_t f, uint8_t r)
        : coords()
    {
        inline auto valid_coords = [](uint8_t x) -> bool { return x >= 0 && x < 64; };

        if (valid_coords(f)) {
            file = f;
        }
        if (valid_coords(r)) {
            rank = r;
        }
    }
    
    bool operator==(const coords &c) const {
        return (file == c.file && rank == c.rank);
    }
};

}

#endif