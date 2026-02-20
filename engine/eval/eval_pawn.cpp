#include "evaluator.hpp"

namespace engine {

int64_t Evaluator::evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    int64_t score = 0;
    const uint64_t allPawns = whitePawns | blackPawns;

    int whiteFileCounts[8] = {};
    int blackFileCounts[8] = {};

    for (int f = 0; f < 8; ++f) {
        const uint64_t fm = FILE_MASKS[f];
        whiteFileCounts[f] = __builtin_popcountll(whitePawns & fm);
        blackFileCounts[f] = __builtin_popcountll(blackPawns & fm);
    }

    for (int f = 0; f < 8; ++f) {
        if (whiteFileCounts[f] > 1) score += (whiteFileCounts[f] - 1) * engine::DOUBLED_PAWN_PENALTY;
        if (blackFileCounts[f] > 1) score -= (blackFileCounts[f] - 1) * engine::DOUBLED_PAWN_PENALTY;
    }

    uint64_t wp = whitePawns;
    while (wp) {
        const int sq = popLSB(wp);
        const int file = chess::Board::fileOf(sq);
        const int rank = chess::Board::rankOf(sq);
        const uint64_t leftSupportMask = (file > 0) ? chess::Board::bitMask(static_cast<uint8_t>(sq + 7)) : 0ULL;
        const uint64_t rightSupportMask = (file < 7) ? chess::Board::bitMask(static_cast<uint8_t>(sq + 9)) : 0ULL;
        const bool hasSupport = (whitePawns & (leftSupportMask | rightSupportMask)) != 0ULL;

        const uint64_t adjFilesMask = ADJACENT_FILES_ONLY[file];
        if ((whitePawns & adjFilesMask) == 0) [[unlikely]] {
            score += engine::ISOLATED_PAWN_PENALTY;
        }

        if (hasSupport) {
            score += 15;
        }

        {
            const int forwardSq = sq - 8;
            const bool isBlocked = (forwardSq >= 0) && (allPawns & chess::Board::bitMask(static_cast<uint8_t>(forwardSq)));

            if (isBlocked) {
                if (!hasSupport) {
                    score += engine::ISOLATED_PAWN_PENALTY / 2;
                }
            }
        }

        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = WHITE_FORWARD_FILL[sq];
        if ((blackPawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score += engine::PASSED_PAWN_BONUS;
            const int advancement = 6 - rank;
            score += advancement * (isEndgame ? 6 : 2);

            if (rank == 1) {
                score += isEndgame ? 40 : 20;
            }

            const int forwardSq = sq - 8;
            if (forwardSq >= 0 && (blackPawns & chess::Board::bitMask(forwardSq))) {
                score -= engine::PASSED_PAWN_BONUS / 2;
            }
            if (isEndgame) {
                score += (6 - rank) * 4;
            }
        }
    }

    uint64_t bp = blackPawns;
    while (bp) {
        const int sq = popLSB(bp);
        const int file = chess::Board::fileOf(sq);
        const int rank = chess::Board::rankOf(sq);
        const uint64_t leftSupportMask = (file > 0 && sq >= 7) ? chess::Board::bitMask(static_cast<uint8_t>(sq - 7)) : 0ULL;
        const uint64_t rightSupportMask = (file < 7 && sq >= 9) ? chess::Board::bitMask(static_cast<uint8_t>(sq - 9)) : 0ULL;
        const bool hasSupport = (blackPawns & (leftSupportMask | rightSupportMask)) != 0ULL;

        const uint64_t adjFilesMask = ADJACENT_FILES_ONLY[file];
        if ((blackPawns & adjFilesMask) == 0) [[unlikely]] {
            score -= engine::ISOLATED_PAWN_PENALTY;
        }

        if (hasSupport) {
            score -= 15;
        }

        {
            const int forwardSq = sq + 8;
            const bool isBlocked = (forwardSq < 64) && (allPawns & chess::Board::bitMask(static_cast<uint8_t>(forwardSq)));

            if (isBlocked) {
                if (!hasSupport) {
                    score -= engine::ISOLATED_PAWN_PENALTY / 2;
                }
            }
        }

        const uint64_t adjAndFileMask = ADJACENT_AND_FILE_MASKS[file];
        const uint64_t forwardMask = BLACK_FORWARD_FILL[sq];
        if ((whitePawns & adjAndFileMask & forwardMask) == 0) [[unlikely]] {
            score -= engine::PASSED_PAWN_BONUS;
            const int advancement = rank - 1;
            score -= advancement * (isEndgame ? 6 : 2);

            if (rank == 6) {
                score -= isEndgame ? 40 : 20;
            }

            const int forwardSq = sq + 8;
            if (forwardSq < 64 && (whitePawns & chess::Board::bitMask(forwardSq))) {
                score += engine::PASSED_PAWN_BONUS / 2;
            }
            if (isEndgame) {
                score -= (rank - 1) * 4;
            }
        }
    }

    return score;
}

int64_t Evaluator::evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    static constexpr uint64_t CENTER_MASK = 0x0000001818000000ULL;
    return (__builtin_popcountll(whitePawns & CENTER_MASK) - __builtin_popcountll(blackPawns & CENTER_MASK)) * engine::CENTER_CONTROL_BONUS;
}

int64_t Evaluator::evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept {
    static constexpr uint64_t WHITE_D4_PAWN = chess::Board::bitMask(27);
    static constexpr uint64_t BLACK_D5_PIECE = chess::Board::bitMask(35);
    static constexpr uint64_t WHITE_BLOCKED_KNIGHTS = chess::Board::bitMask(18) | chess::Board::bitMask(21);
    static constexpr uint64_t WHITE_BLOCKED_BISHOPS = chess::Board::bitMask(19) | chess::Board::bitMask(20);

    static constexpr uint64_t BLACK_D5_PAWN = chess::Board::bitMask(35);
    static constexpr uint64_t WHITE_D4_PIECE = chess::Board::bitMask(27);
    static constexpr uint64_t BLACK_BLOCKED_KNIGHTS = chess::Board::bitMask(42) | chess::Board::bitMask(45);
    static constexpr uint64_t BLACK_BLOCKED_BISHOPS = chess::Board::bitMask(43) | chess::Board::bitMask(44);

    static constexpr int64_t BLOCKED_CENTER_PENALTY = 15;
    static constexpr int64_t BLOCKED_PIECE_PENALTY = 10;

    int64_t score = 0;

    const bool whiteBlocked = (b.pawns_bb[0] & WHITE_D4_PAWN) && (occ & BLACK_D5_PIECE);
    score -= whiteBlocked * BLOCKED_CENTER_PENALTY;
    score -= whiteBlocked * static_cast<bool>(b.knights_bb[0] & WHITE_BLOCKED_KNIGHTS) * BLOCKED_PIECE_PENALTY;
    score -= whiteBlocked * static_cast<bool>(b.bishops_bb[0] & WHITE_BLOCKED_BISHOPS) * BLOCKED_PIECE_PENALTY;

    const bool blackBlocked = (b.pawns_bb[1] & BLACK_D5_PAWN) && (occ & WHITE_D4_PIECE);
    score += blackBlocked * BLOCKED_CENTER_PENALTY;
    score += blackBlocked * static_cast<bool>(b.knights_bb[1] & BLACK_BLOCKED_KNIGHTS) * BLOCKED_PIECE_PENALTY;
    score += blackBlocked * static_cast<bool>(b.bishops_bb[1] & BLACK_BLOCKED_BISHOPS) * BLOCKED_PIECE_PENALTY;

    return score;
}

} // namespace engine
