#include <iostream>
#include <cstdint>

int main() {
    uint64_t val = 1ULL;
    int shift = 64;
    uint64_t res = (val << (shift & 63)) - 1; 
    uint64_t ub = (val << shift) - 1;
    std::cout << "Safe: " << res << " UB: " << ub << std::endl;
}
