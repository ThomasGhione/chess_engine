#include "../evaluator.hpp"

namespace engine {

static constexpr auto WHITE_SUPPORT_MASKS = [] {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) {
        const int file = chess::Board::file(sq);
        uint64_t mask = 0ULL;
        if (file > 0 && sq <= 56) mask |= chess::Board::bitMask(sq + 7);
        if (file < 7 && sq <= 54) mask |= chess::Board::bitMask(sq + 9);
        masks[sq] = mask;
    }
    return masks;
}();

static constexpr auto BLACK_SUPPORT_MASKS = [] {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) {
        const int file = chess::Board::file(sq);
        uint64_t mask = 0ULL;
        if (file > 0 && sq >= 7) mask |= chess::Board::bitMask(sq - 7);
        if (file < 7 && sq >= 9) mask |= chess::Board::bitMask(sq - 9);
        masks[sq] = mask;
    }
    return masks;
}();

static constexpr auto WHITE_ONE_STEP_MASKS = [] {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) {
        masks[sq] = (sq >= 8) ? chess::Board::bitMask(sq - 8) : 0ULL;
    }
    return masks;
}();

static constexpr auto BLACK_ONE_STEP_MASKS = [] {
    std::array<uint64_t, 64> masks{};
    for (int sq = 0; sq < 64; ++sq) {
        masks[sq] = (sq <= 55) ? chess::Board::bitMask(sq + 8) : 0ULL;
    }
    return masks;
}();

inline const std::array<uint64_t, 64>& Evaluator::getPawnSupportMasks(bool isWhite) noexcept {
    return isWhite ? WHITE_SUPPORT_MASKS : BLACK_SUPPORT_MASKS;
}

inline const std::array<uint64_t, 64>& Evaluator::getPawnOneStepMasks(bool isWhite) noexcept {
    return isWhite ? WHITE_ONE_STEP_MASKS : BLACK_ONE_STEP_MASKS;
}

Evaluator::PawnFileStats Evaluator::evalPawnFileStats(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    PawnFileStats stats;
    bool prevWhiteFileOccupied = false;
    bool prevBlackFileOccupied = false;

    for (int f = 0; f < 8; ++f) {
        const uint64_t fileMask = FILE_MASKS[f];
        const uint64_t adjacentMask = ADJACENT_FILES_ONLY[f];
        const uint64_t adjacentAndFileMask = ADJACENT_AND_FILE_MASKS[f];
        const int whiteOnFile = __builtin_popcountll(whitePawns & fileMask);
        const int blackOnFile = __builtin_popcountll(blackPawns & fileMask);
        const bool whiteFileOccupied = (whiteOnFile > 0);
        const bool blackFileOccupied = (blackOnFile > 0);

        if (whiteFileOccupied && !prevWhiteFileOccupied) {
            ++stats.whiteIslands;
        }
        if (blackFileOccupied && !prevBlackFileOccupied) {
            ++stats.blackIslands;
        }
        prevWhiteFileOccupied = whiteFileOccupied;
        prevBlackFileOccupied = blackFileOccupied;

        if (whiteOnFile > 1) stats.doubledScore += (whiteOnFile - 1) * engine::DOUBLED_PAWN_PENALTY;
        if (blackOnFile > 1) stats.doubledScore -= (blackOnFile - 1) * engine::DOUBLED_PAWN_PENALTY;

        stats.whiteIsolatedOnFile[f] = (whitePawns & adjacentMask) == 0ULL;
        stats.blackIsolatedOnFile[f] = (blackPawns & adjacentMask) == 0ULL;

        stats.whiteAdjAndFilePawns[f] = whitePawns & adjacentAndFileMask;
        stats.blackAdjAndFilePawns[f] = blackPawns & adjacentAndFileMask;
    }

    if (stats.whiteIslands > 1) {
        stats.islandScore += (stats.whiteIslands - 1) * engine::PAWN_ISLAND_PENALTY;
    }
    if (stats.blackIslands > 1) {
        stats.islandScore -= (stats.blackIslands - 1) * engine::PAWN_ISLAND_PENALTY;
    }

    return stats;
}

int32_t Evaluator::evalPassedPawn(int sq, int rank, uint64_t ownPawns, uint64_t allPawns,
                                  int file, const uint64_t& forwardFill,
                                  const std::array<uint64_t, 64>& oneStepMasks,
                                  const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                  const uint64_t (&enemyAdjAndFilePawns)[8],
                                  int32_t passedAdvancementScale, int32_t passedNearPromotionBonus,
                                  int32_t connectedPasserBonus, int promotionRank, int sign) noexcept {
    int32_t score = sign * engine::PASSED_PAWN_BONUS;
    const int advancement = sign > 0 ? (6 - rank) : (rank - 1);
    score += sign * (advancement * passedAdvancementScale);

    if (rank == promotionRank) {
        score += sign * passedNearPromotionBonus;
    }

    const uint64_t frontMask = oneStepMasks[sq];
    const bool frontBlockedByPawn = (frontMask != 0ULL) && ((allPawns & frontMask) != 0ULL);
    if (frontBlockedByPawn) {
        score += sign * engine::PASSED_PAWN_BLOCKED_PENALTY;
    }

    bool hasConnectedPassedPawn = false;
    uint64_t adjacentPawns = ownPawns & ADJACENT_FILES_ONLY[file];
    while (adjacentPawns) {
        const int adjSq = popLSB(adjacentPawns);
        const int adjRank = chess::Board::rank(adjSq);
        if (std::abs(adjRank - rank) > 1) continue;

        const int adjFile = chess::Board::file(adjSq);
        const bool adjPassed = ((enemyAdjAndFilePawns[adjFile] & forwardFill) == 0ULL);
        if (adjPassed) {
            hasConnectedPassedPawn = true;
            break;
        }
    }

    if (hasConnectedPassedPawn) {
        score += sign * connectedPasserBonus;
    }

    return score;
}

int32_t Evaluator::evalNonPassedPawn(int rank, uint64_t ownPawns, uint64_t enemyPawns,
                                     uint64_t allPawns, int file, bool hasSupport,
                                     const uint64_t& frontMask, const uint64_t& forwardFill,
                                     const uint8_t (&ownIsolatedOnFile)[8],
                                     const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                     int32_t candidatePasserBonus, int pawnAttackerIndex,
                                     bool isWhite, int sign) noexcept {
    int32_t score = 0;

    const bool noEnemySameFileAhead = ((enemyPawns & FILE_MASKS[file] & forwardFill) == 0ULL);
    if (noEnemySameFileAhead && (frontMask == 0ULL || (allPawns & frontMask) == 0ULL) && hasSupport) {
        const int enemyAdjacentAhead = __builtin_popcountll(
            enemyPawns & ADJACENT_FILES_ONLY[file] & forwardFill);
        if (enemyAdjacentAhead <= 1) {
            score += sign * candidatePasserBonus;
        }
    }

    if (ownIsolatedOnFile[file] != 0 || hasSupport || frontMask == 0ULL) {
        return score;
    }

    const int frontSq = __builtin_ctzll(frontMask);
    const bool frontControlledByEnemyPawn =
        (pieces::PAWN_ATTACKERS_TO[pawnAttackerIndex][frontSq] & enemyPawns) != 0ULL;

    bool hasAdjacentHelper = false;
    uint64_t adjacentHelpers = ownPawns & ADJACENT_FILES_ONLY[file];
    while (adjacentHelpers) {
        const int helperSq = popLSB(adjacentHelpers);
        const int helperRank = chess::Board::rank(helperSq);
        if (isWhite ? (helperRank >= rank) : (helperRank <= rank)) {
            hasAdjacentHelper = true;
            break;
        }
    }

    if (frontControlledByEnemyPawn && !hasAdjacentHelper) {
        score += sign * engine::BACKWARD_PAWN_PENALTY;
    }

    return score;
}

int32_t Evaluator::evalPawnsByColor(uint64_t ownPawns, uint64_t enemyPawns, uint64_t allPawns,
                                    const uint8_t (&ownIsolatedOnFile)[8],
                                    const uint64_t (&enemyAdjAndFilePawns)[8],
                                    int32_t passedAdvancementScale, int32_t passedNearPromotionBonus,
                                    int32_t connectedPasserBonus, int32_t candidatePasserBonus,
                                    int sign) noexcept {
    // Determine support masks and forward fill based on color
    const bool isWhite = (sign > 0);
    
    const auto& supportMasks = getPawnSupportMasks(isWhite);
    const auto& oneStepMasks = getPawnOneStepMasks(isWhite);
    const auto& forwardFill = isWhite ? WHITE_FORWARD_FILL : BLACK_FORWARD_FILL;
    const int pawnAttackerIndex = isWhite ? 1 : 0;
    const int promotionRank = isWhite ? 1 : 6;

    int32_t score = 0;
    uint64_t pawns = ownPawns;
    while (pawns) {
        const int sq = popLSB(pawns);
        const int file = chess::Board::file(sq);
        const int rank = chess::Board::rank(sq);
        const bool hasSupport = (ownPawns & supportMasks[sq]) != 0ULL;
        const uint64_t frontMask = oneStepMasks[sq];
        const bool frontBlockedByPawn = (frontMask != 0ULL) && ((allPawns & frontMask) != 0ULL);
        const uint64_t enemyForwardAdjFile = enemyAdjAndFilePawns[file];
        const bool isPassed = ((enemyForwardAdjFile & forwardFill[sq]) == 0ULL);

        if (ownIsolatedOnFile[file]) {
            score += sign * engine::ISOLATED_PAWN_PENALTY;
        }

        if (hasSupport) {
            score += sign * engine::PAWN_SUPPORT_BONUS;
        } else if (frontBlockedByPawn) {
	    // Suppose that !hasSuppport == true 
            score += sign * (engine::ISOLATED_PAWN_PENALTY / 2);
        }

        if (isPassed) {
            score += evalPassedPawn(sq, rank, ownPawns, allPawns, file, forwardFill[sq],
                                   oneStepMasks, ADJACENT_FILES_ONLY, enemyAdjAndFilePawns,
                                   passedAdvancementScale, passedNearPromotionBonus,
                                   connectedPasserBonus, promotionRank, sign);
	  continue;
        } 
	
	// Suppose isPassed == false
	score += evalNonPassedPawn(rank, ownPawns, enemyPawns, allPawns, file, hasSupport,
				  frontMask, forwardFill[sq], ownIsolatedOnFile,
				  ADJACENT_FILES_ONLY,
				  candidatePasserBonus, pawnAttackerIndex, isWhite, sign);
    }

    return score;
}

bool Evaluator::tryPawnCacheHit(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame,
                               int32_t& outScore) noexcept {
    struct PawnEvalCacheEntry {
        uint64_t whitePawns = 0ULL;
        uint64_t blackPawns = 0ULL;
        int32_t score = 0;
        uint8_t isEndgame = 0;
        uint8_t valid = 0;
        uint16_t stamp = 0;
    };

    constexpr size_t PAWN_CACHE_SIZE = 1u << 13;
    constexpr size_t PAWN_CACHE_WAYS = 2;
    constexpr uint64_t PAWN_CACHE_MASK = PAWN_CACHE_SIZE - 1u;
    thread_local std::array<std::array<PawnEvalCacheEntry, PAWN_CACHE_WAYS>, PAWN_CACHE_SIZE> pawnCache{};
    thread_local uint16_t pawnCacheStamp = 0;

    const uint64_t cacheHash =
        (whitePawns * 0x9E3779B97F4A7C15ULL) ^
        (blackPawns * 0xC2B2AE3D27D4EB4FULL) ^
        isEndgame;
    std::array<PawnEvalCacheEntry, PAWN_CACHE_WAYS>& cacheBucket = pawnCache[cacheHash & PAWN_CACHE_MASK];
    const uint8_t endgameTag = isEndgame;
    const uint16_t currentStamp = ++pawnCacheStamp;

    for (size_t way = 0; way < PAWN_CACHE_WAYS; ++way) {
        PawnEvalCacheEntry& cacheEntry = cacheBucket[way];
        if (cacheEntry.valid
            && cacheEntry.whitePawns == whitePawns
            && cacheEntry.blackPawns == blackPawns
            && cacheEntry.isEndgame == endgameTag) {
            cacheEntry.stamp = currentStamp;
            outScore = cacheEntry.score;
            return true;
        }
    }

    return false;
}

void Evaluator::storePawnEvalCache(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame,
                                  int32_t score) noexcept {
    struct PawnEvalCacheEntry {
        uint64_t whitePawns = 0ULL;
        uint64_t blackPawns = 0ULL;
        int32_t score = 0;
        uint8_t isEndgame = 0;
        uint8_t valid = 0;
        uint16_t stamp = 0;
    };

    constexpr size_t PAWN_CACHE_SIZE = 1u << 13;
    constexpr size_t PAWN_CACHE_WAYS = 2;
    constexpr uint64_t PAWN_CACHE_MASK = PAWN_CACHE_SIZE - 1u;
    thread_local std::array<std::array<PawnEvalCacheEntry, PAWN_CACHE_WAYS>, PAWN_CACHE_SIZE> pawnCache{};
    thread_local uint16_t pawnCacheStamp = 0;

    const uint64_t cacheHash =
        (whitePawns * 0x9E3779B97F4A7C15ULL) ^
        (blackPawns * 0xC2B2AE3D27D4EB4FULL) ^
        isEndgame;
    std::array<PawnEvalCacheEntry, PAWN_CACHE_WAYS>& cacheBucket = pawnCache[cacheHash & PAWN_CACHE_MASK];
    const uint8_t endgameTag = isEndgame;
    const uint16_t currentStamp = ++pawnCacheStamp;

    PawnEvalCacheEntry* replaceEntry = &cacheBucket[0];
    if (!cacheBucket[0].valid) {
        replaceEntry = &cacheBucket[0];
    } else if (!cacheBucket[1].valid) {
        replaceEntry = &cacheBucket[1];
    } else if (cacheBucket[1].stamp < cacheBucket[0].stamp) {
        replaceEntry = &cacheBucket[1];
    }

    replaceEntry->whitePawns = whitePawns;
    replaceEntry->blackPawns = blackPawns;
    replaceEntry->isEndgame = endgameTag;
    replaceEntry->score = score;
    replaceEntry->valid = 1;
    replaceEntry->stamp = currentStamp;
}

int32_t Evaluator::evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept {
    int32_t cachedScore = 0;
    if (tryPawnCacheHit(whitePawns, blackPawns, isEndgame, cachedScore)) {
        return cachedScore;
    }

    const int32_t passedAdvancementScale = isEndgame ? 10 : 2;
    const int32_t passedNearPromotionBonus = isEndgame ? 40 : 20;
    const int32_t connectedPasserBonus = isEndgame
        ? (engine::CONNECTED_PASSER_BONUS + 6)
        : engine::CONNECTED_PASSER_BONUS;
    const int32_t candidatePasserBonus = isEndgame
        ? (engine::CANDIDATE_PASSER_BONUS + 4)
        : engine::CANDIDATE_PASSER_BONUS;

    PawnFileStats fileStats = Evaluator::evalPawnFileStats(whitePawns, blackPawns);
    
    int32_t score = fileStats.doubledScore + fileStats.islandScore;
    const uint64_t allPawns = whitePawns | blackPawns;

    score += Evaluator::evalPawnsByColor(whitePawns, blackPawns, allPawns,
                                         fileStats.whiteIsolatedOnFile, fileStats.blackAdjAndFilePawns,
                                         passedAdvancementScale, passedNearPromotionBonus,
                                         connectedPasserBonus, candidatePasserBonus, 1);

    score += Evaluator::evalPawnsByColor(blackPawns, whitePawns, allPawns,
                                         fileStats.blackIsolatedOnFile, fileStats.whiteAdjAndFilePawns,
                                         passedAdvancementScale, passedNearPromotionBonus,
                                         connectedPasserBonus, candidatePasserBonus, -1);

    storePawnEvalCache(whitePawns, blackPawns, isEndgame, score);

    return score;
}

} // namespace engine
