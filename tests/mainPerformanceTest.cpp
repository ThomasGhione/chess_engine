#include "ut.hpp"
#include "../board/piece.hpp"

void registerPerformanceEngineSuite();

int main(){
    // Initialize magic bitboards before running performance tests
    pieces::initMagicBitboards();
    registerPerformanceEngineSuite();
}
