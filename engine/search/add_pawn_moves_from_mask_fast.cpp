#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

void Engine::addPawnMovesFromMaskFast(const chess::Board& b,
                                     MoveList<chess::Board::Move>& moves,
                                     uint8_t from,
                                     uint64_t mask,
                                     bool inCheck,
                                     bool inDoubleCheck,
                                     uint8_t fromPiece,
                                     bool skipLegalityCheck,
                                     uint8_t promotionRank,
                                     const chess::Coords& enPassant) noexcept {
    if (!mask) [[unlikely]] return;
    const chess::Coords fromC{from};
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        const chess::Coords toC{to};
        const bool isEnPassant = hasEnPassant
            && (toC == enPassant)
            && (static_cast<uint8_t>(to & 7) != fromFile);
        if ((!skipLegalityCheck || isEnPassant) && (!b.isLegalPseudoMove(from, to, fromPiece, inCheck, inDoubleCheck)) ){
                continue;
        }

        if (chess::Board::rankOf(to) == promotionRank) {
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
        } else {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}
} // namespace engine
