#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

void Engine::addNonPawnMovesFromMaskFast(const chess::Board& b,
                                        MoveList<chess::Board::Move>& moves,
                                        uint8_t from,
                                        uint64_t mask,
                                        bool inCheck,
                                        bool inDoubleCheck,
                                        uint8_t fromPiece,
                                        bool skipLegalityCheck) noexcept {
    if (!mask) [[unlikely]] return;

    const chess::Coords fromC{from};
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        if (skipLegalityCheck || b.isLegalPseudoMove(from, to, fromPiece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }
}

} // namespace engine
