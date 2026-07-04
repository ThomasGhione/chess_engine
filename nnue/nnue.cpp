#include "nnue.hpp"

namespace NNUE {

bool networkLoaded() noexcept {
    return false; // Fase 3: network loading + inference
}

int32_t evaluate(const chess::Board&) noexcept {
    // Unreachable while networkLoaded() is false: UCI keeps `enabled` off.
    return 0;
}

} // namespace NNUE
