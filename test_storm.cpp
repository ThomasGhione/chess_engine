#include <iostream>
#include <cstdint>
int main() {
    for (int r = 1; r < 7; ++r) {
        uint64_t mW = ~0ULL << (r * 8);
        uint64_t mB = (1ULL << ((r + 1) * 8)) - 1;
        std::cout << "r=" << r << " mW=" << std::hex << mW << " mB=" << mB << std::endl;
    }
}
