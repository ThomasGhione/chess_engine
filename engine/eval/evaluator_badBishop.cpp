#include "evaluator.hpp"
namespace engine {

template<int Side>
inline constexpr int64_t Evaluator::evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
    static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");

    const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
    const int lightPawnCount = __builtin_popcountll(pawns & LIGHT_SQUARES);

    const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
    const int lightBishops = __builtin_popcountll(bishops & LIGHT_SQUARES);

    const int64_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * 8);

    if constexpr (Side == 0) {
        return score;
    } else {
        return -score;
    }
}

int64_t Evaluator::evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept {
    return (side == 0) ? Evaluator::evalBadBishopImpl<0>(bishops, pawns) : Evaluator::evalBadBishopImpl<1>(bishops, pawns);
}
} // namespace engine
