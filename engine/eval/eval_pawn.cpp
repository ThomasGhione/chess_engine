#include "evaluator.hpp"

namespace engine {

int64_t Evaluator::evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    struct PawnEvalCacheEntry {
        uint64_t whitePawns = 0ULL;
        uint64_t blackPawns = 0ULL;
        int64_t score = 0;
        uint8_t isEndgame = 0;
    };

    constexpr size_t PAWN_CACHE_SIZE = 1u << 13; // 8192 entries, ~128KB total (fits in L1 cache)
    constexpr uint64_t PAWN_CACHE_MASK = static_cast<uint64_t>(PAWN_CACHE_SIZE - 1u);
    thread_local std::array<PawnEvalCacheEntry, PAWN_CACHE_SIZE> pawnCache{};

    const uint64_t cacheHash =
        (whitePawns * 0x9E3779B97F4A7C15ULL) ^
        (blackPawns * 0xC2B2AE3D27D4EB4FULL) ^
        static_cast<uint64_t>(isEndgame);
    PawnEvalCacheEntry& cacheEntry = pawnCache[cacheHash & PAWN_CACHE_MASK];
    const uint8_t endgameTag = static_cast<uint8_t>(isEndgame);
    if (cacheEntry.whitePawns == whitePawns
        && cacheEntry.blackPawns == blackPawns
        && cacheEntry.isEndgame == endgameTag) {
        return cacheEntry.score;
    }

    static constexpr auto WHITE_SUPPORT_MASKS = [] {
        std::array<uint64_t, 64> masks{};
        for (int sq = 0; sq < 64; ++sq) {
            const int file = sq & 7;
            uint64_t mask = 0ULL;
            if (file > 0 && sq <= 56) mask |= chess::Board::bitMask(static_cast<uint8_t>(sq + 7));
            if (file < 7 && sq <= 54) mask |= chess::Board::bitMask(static_cast<uint8_t>(sq + 9));
            masks[sq] = mask;
        }
        return masks;
    }();

    static constexpr auto BLACK_SUPPORT_MASKS = [] {
        std::array<uint64_t, 64> masks{};
        for (int sq = 0; sq < 64; ++sq) {
            const int file = sq & 7;
            uint64_t mask = 0ULL;
            if (file > 0 && sq >= 7) mask |= chess::Board::bitMask(static_cast<uint8_t>(sq - 7));
            if (file < 7 && sq >= 9) mask |= chess::Board::bitMask(static_cast<uint8_t>(sq - 9));
            masks[sq] = mask;
        }
        return masks;
    }();

    static constexpr auto WHITE_ONE_STEP_MASKS = [] {
        std::array<uint64_t, 64> masks{};
        for (int sq = 0; sq < 64; ++sq) {
            masks[sq] = (sq >= 8) ? chess::Board::bitMask(static_cast<uint8_t>(sq - 8)) : 0ULL;
        }
        return masks;
    }();

    static constexpr auto BLACK_ONE_STEP_MASKS = [] {
        std::array<uint64_t, 64> masks{};
        for (int sq = 0; sq < 64; ++sq) {
            masks[sq] = (sq <= 55) ? chess::Board::bitMask(static_cast<uint8_t>(sq + 8)) : 0ULL;
        }
        return masks;
    }();

    int64_t score = 0;
    const uint64_t allPawns = whitePawns | blackPawns;
    const int64_t passedAdvancementScale = isEndgame ? 10 : 2;
    const int64_t passedNearPromotionBonus = isEndgame ? 40 : 20;

    uint8_t whiteIsolatedOnFile[8] = {};
    uint8_t blackIsolatedOnFile[8] = {};
    uint64_t whiteAdjAndFilePawns[8] = {};
    uint64_t blackAdjAndFilePawns[8] = {};

    for (int f = 0; f < 8; ++f) {
        const uint64_t fileMask = FILE_MASKS[f];
        const uint64_t adjacentMask = ADJACENT_FILES_ONLY[f];
        const uint64_t adjacentAndFileMask = ADJACENT_AND_FILE_MASKS[f];
        const int whiteOnFile = __builtin_popcountll(whitePawns & fileMask);
        const int blackOnFile = __builtin_popcountll(blackPawns & fileMask);

        if (whiteOnFile > 1) score += (whiteOnFile - 1) * engine::DOUBLED_PAWN_PENALTY;
        if (blackOnFile > 1) score -= (blackOnFile - 1) * engine::DOUBLED_PAWN_PENALTY;

        whiteIsolatedOnFile[f] = static_cast<uint8_t>((whitePawns & adjacentMask) == 0ULL);
        blackIsolatedOnFile[f] = static_cast<uint8_t>((blackPawns & adjacentMask) == 0ULL);

        whiteAdjAndFilePawns[f] = whitePawns & adjacentAndFileMask;
        blackAdjAndFilePawns[f] = blackPawns & adjacentAndFileMask;
    }

    uint64_t wp = whitePawns;
    while (wp) {
        const int sq = popLSB(wp);
        const int file = sq & 7;
        const int rank = sq >> 3;
        const bool hasSupport = (whitePawns & WHITE_SUPPORT_MASKS[sq]) != 0ULL;
        const uint64_t blackForwardAdjFile = blackAdjAndFilePawns[file];

        if (whiteIsolatedOnFile[file]) {
            score += engine::ISOLATED_PAWN_PENALTY;
        }

        if (hasSupport) {
            score += 15;
        }

        if (allPawns & WHITE_ONE_STEP_MASKS[sq]) {
            if (!hasSupport) {
                score += engine::ISOLATED_PAWN_PENALTY / 2;
            }
        }

        if ((blackForwardAdjFile & WHITE_FORWARD_FILL[sq]) == 0ULL) {
            score += engine::PASSED_PAWN_BONUS;
            const int advancement = 6 - rank;
            score += advancement * passedAdvancementScale;

            if (rank == 1) {
                score += passedNearPromotionBonus;
            }
        }
    }

    uint64_t bp = blackPawns;
    while (bp) {
        const int sq = popLSB(bp);
        const int file = sq & 7;
        const int rank = sq >> 3;
        const bool hasSupport = (blackPawns & BLACK_SUPPORT_MASKS[sq]) != 0ULL;
        const uint64_t whiteForwardAdjFile = whiteAdjAndFilePawns[file];

        if (blackIsolatedOnFile[file]) {
            score -= engine::ISOLATED_PAWN_PENALTY;
        }

        if (hasSupport) {
            score -= 15;
        }

        if (allPawns & BLACK_ONE_STEP_MASKS[sq]) {
            if (!hasSupport) {
                score -= engine::ISOLATED_PAWN_PENALTY / 2;
            }
        }

        if ((whiteForwardAdjFile & BLACK_FORWARD_FILL[sq]) == 0ULL) {
            score -= engine::PASSED_PAWN_BONUS;
            const int advancement = rank - 1;
            score -= advancement * passedAdvancementScale;

            if (rank == 6) {
                score -= passedNearPromotionBonus;
            }
        }
    }

    cacheEntry.whitePawns = whitePawns;
    cacheEntry.blackPawns = blackPawns;
    cacheEntry.isEndgame = endgameTag;
    cacheEntry.score = score;

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
