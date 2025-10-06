#ifndef COORDS
#define COORDS

#include <string>
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
    
    coords(std::string input)
        : coords()
    {
        inline auto valid_coords = [](uint8_t x) -> bool { return x >= 0 && x < 64; };

        if (input.length() != 2) {
            // TODO: gestire l'errore
        }

        if (valid_coords(input[0] - 'a')) {
            file = input[0] - 'a';
        }
        if (valid_coords(input[1] - '1')) {
            rank = input[1] - '1';
        }
    }

    bool operator==(const coords &c) const {
        return (file == c.file && rank == c.rank);
    }
};

}

#endif