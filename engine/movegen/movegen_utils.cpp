#include "movegen.hpp"

namespace engine {

// ============================================================================
// Helper function: betweenMaskExclusive
// ============================================================================
// Computes the mask of squares between two squares (exclusive)
static inline uint64_t betweenMaskExclusive(uint8_t from, uint8_t to) noexcept {
    if (from == to) [[unlikely]] return 0ULL;

    const int fromFile = chess::Board::fileOf(from);
    const int fromRank = chess::Board::rankOf(from);
    const int toFile = chess::Board::fileOf(to);
    const int toRank = chess::Board::rankOf(to);
    const int df = toFile - fromFile;
    const int dr = toRank - fromRank;

    int stepFile = 0;
    int stepRank = 0;
    if (df == 0) {
        stepRank = (dr > 0) ? 1 : -1;
    } else if (dr == 0) {
        stepFile = (df > 0) ? 1 : -1;
    } else if ((df > 0 ? df : -df) == (dr > 0 ? dr : -dr)) {
        stepFile = (df > 0) ? 1 : -1;
        stepRank = (dr > 0) ? 1 : -1;
    } else {
        return 0ULL;
    }

    uint64_t mask = 0ULL;
    int f = fromFile + stepFile;
    int r = fromRank + stepRank;
    while (f != toFile || r != toRank) {
        mask |= chess::Board::bitMask(static_cast<uint8_t>((r << 3) | f));
        f += stepFile;
        r += stepRank;
    }

    mask &= ~chess::Board::bitMask(to);
    return mask;
}

// ============================================================================
// computePinRays
// ============================================================================
// Returns a mask with bits for pieces pinned to the king (pinnedMask)
// and an array that stores the pin-ray mask for each square (pinRayBySquare).
void MoveGenerator::computePinRays(
    const chess::Board& b,
    chess::Coords kingPos,
    bool isWhite,
    uint64_t& pinnedMask,
    uint64_t pinRays[64]) noexcept {
    
    pinnedMask = 0ULL;
    const int us = isWhite ? 0 : 1;
    const int them = us ^ 1;
    
    // Initialize all pin rays to 0
    for (int i = 0; i < 64; ++i) {
        pinRays[i] = 0ULL;
    }
    
    const uint8_t kingSq = kingPos.index;
    const uint8_t ownColor = isWhite ? chess::Board::WHITE : chess::Board::BLACK;
    const uint64_t rookLikeEnemy = b.rooks_bb[them] | b.queens_bb[them];
    const uint64_t bishopLikeEnemy = b.bishops_bb[them] | b.queens_bb[them];
    
    if ((rookLikeEnemy | bishopLikeEnemy) == 0ULL) {
        return;
    }

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
            if (pieceColor == ownColor) {  // Own piece
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
                    pinnedMask |= chess::Board::bitMask(static_cast<uint8_t>(pinnedSq));
                    pinRays[static_cast<size_t>(pinnedSq)] =
                        betweenMaskExclusive(kingSq, sq) | chess::Board::bitMask(sq);
                }
            }
            break;
        }
    }
}

// ============================================================================
// computeCheckEvasionMasks
// ============================================================================
// Returns a mask with bits for squares where pieces can move or interpose
// to evade check (evasionMask).
void MoveGenerator::computeCheckEvasionMasks(
    const chess::Board& b,
    uint8_t color,
    bool inCheck,
    bool inDoubleCheck,
    uint64_t& evasionMask) noexcept {
    
    evasionMask = ~0ULL;

    if (!inCheck) return;

    const int us = chess::Board::colorToIndex(color);
    const int them = us ^ 1;
    const uint64_t kingBB = b.kings_bb[us];
    if (!kingBB) [[unlikely]] {
        evasionMask = 0ULL;
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
        evasionMask = 0ULL;
        return;
    }

    if (!checkersMask) [[unlikely]] {
        evasionMask = ~0ULL;
        return;
    }

    const uint8_t checkerSq = static_cast<uint8_t>(__builtin_ctzll(checkersMask));
    const uint8_t checkerType = b.get(checkerSq) & chess::Board::MASK_PIECE_TYPE;

    evasionMask = chess::Board::bitMask(checkerSq);
    if (checkerType == chess::Board::ROOK
        || checkerType == chess::Board::BISHOP
        || checkerType == chess::Board::QUEEN) {
        evasionMask |= betweenMaskExclusive(kingSq, checkerSq);
    }
}

} // namespace engine
