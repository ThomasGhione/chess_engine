#include "../evaluator.hpp"

namespace engine {

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

inline const std::array<uint64_t, 64>& Evaluator::getPawnSupportMasks(bool isWhite) noexcept {
    // Friendly pawns that defend a pawn on sq == enemy-perspective pawn
    // attackers to sq for the same color. Reuses the engine-wide table.
    return pieces::PAWN_ATTACKERS_TO[isWhite ? 0 : 1];
}

inline const std::array<uint64_t, 64>& Evaluator::getPawnOneStepMasks(bool isWhite) noexcept {
    return pieces::PAWN_SINGLE_PUSH_TARGETS[isWhite ? 0 : 1];
}

inline uint8_t pawnFileMask(uint64_t pawns) noexcept {
    pawns |= pawns >> 32;
    pawns |= pawns >> 16;
    pawns |= pawns >> 8;
    return static_cast<uint8_t>(pawns & 0xFF);
}

Evaluator::PawnFileStats Evaluator::evalPawnFileStats(uint64_t whitePawns, uint64_t blackPawns) noexcept {
    PawnFileStats stats;

    const uint8_t wFiles = pawnFileMask(whitePawns);
    const uint8_t bFiles = pawnFileMask(blackPawns);

    // Isolated files bitmask (file is isolated if adjacent files have no pawns)
    stats.whiteIsolatedFiles = static_cast<uint8_t>(~((wFiles << 1) | (wFiles >> 1)));
    stats.blackIsolatedFiles = static_cast<uint8_t>(~((bFiles << 1) | (bFiles >> 1)));

    // Islands counts as number of consecutive 1s blocks in wFiles/bFiles
    stats.whiteIslands = __builtin_popcountll(static_cast<unsigned int>(wFiles & static_cast<uint8_t>(~(wFiles << 1))));
    stats.blackIslands = __builtin_popcountll(static_cast<unsigned int>(bFiles & static_cast<uint8_t>(~(bFiles << 1))));

    if (stats.whiteIslands > 1) {
        stats.islandScore += (stats.whiteIslands - 1) * engine::PAWN_ISLAND_PENALTY;
    }
    if (stats.blackIslands > 1) {
        stats.islandScore -= (stats.blackIslands - 1) * engine::PAWN_ISLAND_PENALTY;
    }

    const int totalWhitePawns = __builtin_popcountll(whitePawns);
    const int totalBlackPawns = __builtin_popcountll(blackPawns);
    const int whiteOccupiedFiles = __builtin_popcountll(static_cast<unsigned int>(wFiles));
    const int blackOccupiedFiles = __builtin_popcountll(static_cast<unsigned int>(bFiles));

    stats.doubledScore += (totalWhitePawns - whiteOccupiedFiles) * engine::DOUBLED_PAWN_PENALTY;
    stats.doubledScore -= (totalBlackPawns - blackOccupiedFiles) * engine::DOUBLED_PAWN_PENALTY;

    return stats;
}

int32_t Evaluator::evalPassedPawn(int sq, int rank, uint64_t ownPawns, uint64_t allPawns,
                                  int file, const uint64_t& forwardFill,
                                  const std::array<uint64_t, 64>& oneStepMasks,
                                  const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                  uint64_t enemyPawns,
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
    // L'uso di pieces::KING_ATTACKS[sq] sostituisce il check (std::abs(adjRank - rank) <= 1)
    // eliminando il branch all'interno del loop e mascherando direttamente in hardware
    uint64_t adjacentPawns = ownPawns & ADJACENT_FILES_ONLY[file] & pieces::KING_ATTACKS[sq];
    while (adjacentPawns) {
        const int adjSq = popLSB(adjacentPawns);
        const int adjFile = chess::Board::file(adjSq);
        const bool adjPassed = ((enemyPawns & ADJACENT_AND_FILE_MASKS[adjFile] & forwardFill) == 0ULL);
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
                                     uint8_t ownIsolatedFiles,
                                     const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                     int32_t candidatePasserBonus, int pawnAttackerIndex,
                                     bool isWhite, int sign) noexcept {
    int32_t score = 0;

    const bool noEnemySameFileAhead = ((enemyPawns & FILE_MASKS[file] & forwardFill) == 0ULL);
    if (noEnemySameFileAhead && (frontMask == 0ULL || (allPawns & frontMask) == 0ULL) && hasSupport) {
        const uint64_t enemyAdjacentAhead = enemyPawns & ADJACENT_FILES_ONLY[file] & forwardFill;
        if ((enemyAdjacentAhead & (enemyAdjacentAhead - 1ULL)) == 0ULL) {
            score += sign * candidatePasserBonus;
        }
    }

    if (((ownIsolatedFiles & (1 << file)) != 0) || hasSupport || frontMask == 0ULL) {
        return score;
    }

    const int frontSq = __builtin_ctzll(frontMask);
    const bool frontControlledByEnemyPawn =
        (pieces::PAWN_ATTACKERS_TO[pawnAttackerIndex][frontSq] & enemyPawns) != 0ULL;

    const uint64_t helperRankMask = isWhite
        ? ~RANK_BELOW_MASKS[rank]
        : ~RANK_ABOVE_MASKS[rank];
    const bool hasAdjacentHelper = (ownPawns & ADJACENT_FILES_ONLY[file] & helperRankMask) != 0ULL;

    if (frontControlledByEnemyPawn && !hasAdjacentHelper) {
        score += sign * engine::BACKWARD_PAWN_PENALTY;
    }

    return score;
}

int32_t Evaluator::evalPawnsByColor(uint64_t ownPawns, uint64_t enemyPawns, uint64_t allPawns,
                                    uint8_t ownIsolatedFiles,
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
        const uint64_t enemyForwardAdjFile = enemyPawns & ADJACENT_AND_FILE_MASKS[file];
        const bool isPassed = ((enemyForwardAdjFile & forwardFill[sq]) == 0ULL);

        if ((ownIsolatedFiles & (1 << file)) != 0) {
            score += sign * engine::ISOLATED_PAWN_PENALTY;
        }

        if (hasSupport) {
            score += sign * engine::PAWN_SUPPORT_BONUS;
        } else if (frontBlockedByPawn && (ownIsolatedFiles & (1 << file)) == 0) {
            // Blocked & unsupported (and not already penalized as isolated):
            // half the isolated penalty. Guard prevents 1.5x double-counting
            // on pawns that are both isolated and blocked.
            score += sign * (engine::ISOLATED_PAWN_PENALTY / 2);
        }

        if (isPassed) {
            score += evalPassedPawn(sq, rank, ownPawns, allPawns, file, forwardFill[sq],
                                   oneStepMasks, ADJACENT_FILES_ONLY, enemyPawns,
                                   passedAdvancementScale, passedNearPromotionBonus,
                                   connectedPasserBonus, promotionRank, sign);
	    continue;
        } 
	
	// Suppose isPassed == false
	score += evalNonPassedPawn(rank, ownPawns, enemyPawns, allPawns, file, hasSupport,
				  frontMask, forwardFill[sq], ownIsolatedFiles,
				  ADJACENT_FILES_ONLY,
				  candidatePasserBonus, pawnAttackerIndex, isWhite, sign);
    }

    return score;
}

bool Evaluator::tryPawnCacheHit(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame,
                               int32_t& outScore) noexcept {
    const uint64_t cacheHash =
        (whitePawns * 0x9E3779B97F4A7C15ULL) ^
        (blackPawns * 0xC2B2AE3D27D4EB4FULL) ^
        isEndgame;
    std::array<PawnEvalCacheEntry, PAWN_CACHE_WAYS>& cacheBucket = pawnCache[cacheHash & PAWN_CACHE_MASK];
    const uint8_t endgameTag = isEndgame;

    for (size_t way = 0; way < PAWN_CACHE_WAYS; ++way) {
        PawnEvalCacheEntry& cacheEntry = cacheBucket[way];
        if (cacheEntry.valid
            && cacheEntry.whitePawns == whitePawns
            && cacheEntry.blackPawns == blackPawns
            && cacheEntry.isEndgame == endgameTag) {
            cacheEntry.stamp = ++pawnCacheStamp;
            outScore = cacheEntry.score;
            return true;
        }
    }

    return false;
}

void Evaluator::storePawnEvalCache(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame,
                                  int32_t score) noexcept {
    const uint64_t cacheHash =
        (whitePawns * 0x9E3779B97F4A7C15ULL) ^
        (blackPawns * 0xC2B2AE3D27D4EB4FULL) ^
        isEndgame;
    std::array<PawnEvalCacheEntry, PAWN_CACHE_WAYS>& cacheBucket = pawnCache[cacheHash & PAWN_CACHE_MASK];
    const uint8_t endgameTag = isEndgame;
    const uint16_t currentStamp = ++pawnCacheStamp;

    PawnEvalCacheEntry* replaceEntry = &cacheBucket[0];
    if (!cacheBucket[1].valid || cacheBucket[1].stamp < cacheBucket[0].stamp) {
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
                                         fileStats.whiteIsolatedFiles,
                                         passedAdvancementScale, passedNearPromotionBonus,
                                         connectedPasserBonus, candidatePasserBonus, 1);

    score += Evaluator::evalPawnsByColor(blackPawns, whitePawns, allPawns,
                                         fileStats.blackIsolatedFiles,
                                         passedAdvancementScale, passedNearPromotionBonus,
                                         connectedPasserBonus, candidatePasserBonus, -1);

    storePawnEvalCache(whitePawns, blackPawns, isEndgame, score);

    return score;
}

} // namespace engine
