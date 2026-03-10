#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {
// Returns a mask with bits for squares where pieces can move or interpose
// to evade check (evasionMask).
void Engine::computeCheckEvasionMasks(const chess::Board& b,
                                     uint8_t activeColor,
                                     bool inCheck,
                                     bool inDoubleCheck,
                                     uint64_t& outEvasionMask) noexcept {
    outEvasionMask = ~0ULL;

    if (!inCheck) return;

    const int us = chess::Board::colorToIndex(activeColor);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        outEvasionMask = 0ULL;
        return;
    }

    const uint8_t kingSq = static_cast<uint8_t>(__builtin_ctzll(kingBB));
    const uint64_t occ = b.getPiecesBitMap();

    uint64_t checkersMask = 0ULL;
    checkersMask |= pieces::PAWN_ATTACKERS_TO[us][kingSq] & b.pawns_bb[them];
    checkersMask |= pieces::KNIGHT_ATTACKS[kingSq] & b.knights_bb[them];
    checkersMask |= pieces::KING_ATTACKS[kingSq] & b.kings_bb[them];
    checkersMask |= pieces::getRookAttacks(kingSq, occ) & (b.rooks_bb[them] | b.queens_bb[them]);
    checkersMask |= pieces::getBishopAttacks(kingSq, occ) & (b.bishops_bb[them] | b.queens_bb[them]);

    if (inDoubleCheck || ((checkersMask & (checkersMask - 1)) != 0ULL)) {
        outEvasionMask = 0ULL;
        return;
    }

    if (!checkersMask) [[unlikely]] {
        outEvasionMask = ~0ULL;
        return;
    }

    const uint8_t checkerSq = static_cast<uint8_t>(__builtin_ctzll(checkersMask));
    const uint8_t checkerType = b.get(checkerSq) & chess::Board::MASK_PIECE_TYPE;

    outEvasionMask = chess::Board::bitMask(checkerSq);
    if (checkerType == chess::Board::ROOK
        || checkerType == chess::Board::BISHOP
        || checkerType == chess::Board::QUEEN) {
        outEvasionMask |= betweenMaskExclusive(kingSq, checkerSq);
    }
}

} // namespace engine
