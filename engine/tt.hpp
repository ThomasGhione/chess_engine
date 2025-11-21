#ifndef ENGINE_TT_HPP
#define ENGINE_TT_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace engine {


struct TTEntry {
    uint64_t key = 0;          // 8 bytes
    uint16_t depth = 0;       // 2 bytes
    int32_t score = 0;        // 4 bytes
    uint8_t flag = 0;         // 1 byte
    uint8_t padding[5] = {0}; // 5 bytes to align to 8 bytes

    enum Flag : uint8_t {
        EXACT = 0,
        LOWERBOUND = 1,
        UPPERBOUND = 2
    };
};



} // namespace engine


#endif