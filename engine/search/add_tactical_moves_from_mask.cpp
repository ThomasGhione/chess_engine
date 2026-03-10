#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

// Returns a mask with bits for pieces pinned to the king (pinnedMask)
// and an array that stores the pin-ray mask for each square (pinRayBySquare).
void Engine::computePinRays(const chess::Board& b,
                           uint8_t activeColor,
                           uint64_t& outPinnedMask,
                           std::array<uint64_t, 64>& outPinRayBySquare) noexcept {
    outPinnedMask = 0ULL;
    outPinRayBySquare.fill(0ULL);

    const int us = chess::Board::colorToIndex(activeColor);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        return;
    }

    const uint64_t rookLikeEnemy = b.rooks_bb[them] | b.queens_bb[them];
    const uint64_t bishopLikeEnemy = b.bishops_bb[them] | b.queens_bb[them];
    if ((rookLikeEnemy | bishopLikeEnemy) == 0ULL) {
        return;
    }

    const uint8_t kingSq = static_cast<uint8_t>(__builtin_ctzll(kingBB));
    // Fast bailout: if no enemy slider is even geometrically aligned with the king,
    // no pin can exist and we can skip directional scans entirely.
    if (((pieces::getRookAttacks(kingSq, 0ULL) & rookLikeEnemy) |
         (pieces::getBishopAttacks(kingSq, 0ULL) & bishopLikeEnemy)) == 0ULL) {
        return;
    }
    const int kingFile = chess::Board::fileOf(kingSq);
    const int kingRank = chess::Board::rankOf(kingSq);

    static constexpr int DIRS[8][2] = {
        {0, 1}, {0, -1}, {1, 0}, {-1, 0},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (const auto* dir : DIRS) {
        const int df = dir[0];
        const int dr = dir[1];
        const bool orthogonal = (df == 0 || dr == 0);

        int f = kingFile + df;
        int r = kingRank + dr;
        int pinnedSq = -1;

        while (static_cast<unsigned>(f) < 8U && static_cast<unsigned>(r) < 8U) {
            const uint8_t sq = static_cast<uint8_t>((r << 3) | f);
            const uint8_t piece = b.get(sq);

            if (piece == chess::Board::EMPTY) {
                f += df;
                r += dr;
                continue;
            }

            const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;
            if (pieceColor == activeColor) {
                if (pinnedSq >= 0) {
                    break;
                }
                pinnedSq = sq;
                f += df;
                r += dr;
                continue;
            }

            if (pinnedSq >= 0) {
                const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
                const bool isPinner = orthogonal
                    ? (pieceType == chess::Board::ROOK || pieceType == chess::Board::QUEEN)
                    : (pieceType == chess::Board::BISHOP || pieceType == chess::Board::QUEEN);
                if (isPinner) {
                    outPinnedMask |= chess::Board::bitMask(static_cast<uint8_t>(pinnedSq));
                    outPinRayBySquare[static_cast<size_t>(pinnedSq)] =
                        betweenMaskExclusive(kingSq, sq) | chess::Board::bitMask(sq);
                }
            }
            break;
        }
    }
}
} // namespace engine
