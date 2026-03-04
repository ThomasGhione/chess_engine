#include "evaluator.hpp"

namespace engine {

static constexpr size_t ATTACK_CACHE_SETS = 1u << 10;
static constexpr size_t ATTACK_CACHE_WAYS = 2u;
static constexpr uint64_t ATTACK_CACHE_MASK = static_cast<uint64_t>(ATTACK_CACHE_SETS - 1u);

void Evaluator::computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
    struct AttackCacheEntry {
        uint64_t key = 0ULL;
        AttackData value[2]{};
        uint8_t valid = 0;
        uint8_t age = 0;
    };

    thread_local std::array<std::array<AttackCacheEntry, ATTACK_CACHE_WAYS>, ATTACK_CACHE_SETS> attackCache{};
    thread_local uint8_t cacheClock = 0;

    const uint64_t cacheKey = b.getHash();
    auto& cacheSet = attackCache[(cacheKey * 0x9E3779B97F4A7C15ULL) & ATTACK_CACHE_MASK];

    for (size_t way = 0; way < ATTACK_CACHE_WAYS; ++way) {
        AttackCacheEntry& entry = cacheSet[way];
        if (entry.valid && entry.key == cacheKey) {
            data[0] = entry.value[0];
            data[1] = entry.value[1];
            entry.age = ++cacheClock;
            return;
        }
    }

    data[0] = AttackData{};
    data[1] = AttackData{};

    for (int side = 0; side < 2; ++side) {
        AttackData& d = data[side];
        const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                                b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];
        const uint64_t mobilityMask = ~ownOcc;

        uint64_t pawns = b.pawns_bb[side];
        const bool isWhite = (side == 0);
        while (pawns) {
            const int sq = popLSB(pawns);
            d.allAttacks |= pieces::PAWN_ATTACKS[isWhite][sq];
        }

        uint64_t knights = b.knights_bb[side];
        while (knights) {
            const int sq = popLSB(knights);
            const uint64_t attacks = pieces::KNIGHT_ATTACKS[sq];
            d.allAttacks |= attacks;
            const int mobility = __builtin_popcountll(attacks & mobilityMask);
            d.knightMobility += mobility;
        }

        uint64_t bishops = b.bishops_bb[side];
        while (bishops) {
            const int sq = popLSB(bishops);
            const uint64_t attacks = pieces::getBishopAttacks(sq, occ);
            d.allAttacks |= attacks;
            d.bishopMobility += __builtin_popcountll(attacks & mobilityMask);
        }

        uint64_t rooks = b.rooks_bb[side];
        while (rooks) {
            const int sq = popLSB(rooks);
            const uint64_t attacks = pieces::getRookAttacks(sq, occ);
            d.allAttacks |= attacks;
            d.rookMobility += __builtin_popcountll(attacks & mobilityMask);
        }

        uint64_t queens = b.queens_bb[side];
        while (queens) {
            const int sq = popLSB(queens);
            const uint64_t attacks = pieces::getQueenAttacks(sq, occ);
            d.allAttacks |= attacks;
            d.queenMobility += __builtin_popcountll(attacks & mobilityMask);
        }
    }

    AttackCacheEntry* replace = &cacheSet[0];
    for (size_t way = 0; way < ATTACK_CACHE_WAYS; ++way) {
        AttackCacheEntry& entry = cacheSet[way];
        if (!entry.valid) {
            replace = &entry;
            break;
        }
        if (entry.age < replace->age) {
            replace = &entry;
        }
    }

    replace->key = cacheKey;
    replace->value[0] = data[0];
    replace->value[1] = data[1];
    replace->valid = 1;
    replace->age = ++cacheClock;
}

int64_t Evaluator::evalMobility(const AttackData data[2]) noexcept {
    return (data[0].knightMobility + data[0].bishopMobility + data[0].rookMobility + data[0].queenMobility
          - data[1].knightMobility - data[1].bishopMobility - data[1].rookMobility - data[1].queenMobility) / 2;
}

inline int64_t Evaluator::evalHangingPiecePenalty(uint64_t pieces, uint64_t enemyAttacks, uint64_t friendlyDef, int sign, int penalty) noexcept {
    const uint64_t hanging = pieces & enemyAttacks & ~friendlyDef;
    return sign * __builtin_popcountll(hanging) * penalty;
}

inline int64_t Evaluator::evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept {
    int64_t score = 0;
    const int opp  = side ^ 1;

    const uint64_t enemyAttacks = data[opp].allAttacks;
    const uint64_t friendlyDef = data[side].allAttacks;
    const uint64_t hangingPawns = b.pawns_bb[side] & enemyAttacks & ~friendlyDef;

    score += sign * __builtin_popcountll(hangingPawns) * engine::HANGING_PAWN_PENALTY;

    const uint64_t kingBB = b.kings_bb[side];
    if (kingBB) {
        const int kingSq = __builtin_ctzll(kingBB);
        const uint64_t kingProximity = KING_PROXIMITY_MASKS[kingSq];
        const uint64_t criticalHangingPawns = hangingPawns & kingProximity;
        score += sign * __builtin_popcountll(criticalHangingPawns) * engine::HANGING_PAWN_NEAR_KING_PENALTY;

        const uint64_t hookFiles = FILE_MASKS[1] | FILE_MASKS[6];
        const uint64_t hangingHookPawns = criticalHangingPawns & hookFiles;
        score += sign * __builtin_popcountll(hangingHookPawns) * engine::HANGING_HOOK_PAWN_PENALTY;
    }

    score += evalHangingPiecePenalty(b.knights_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_MINOR_PENALTY);
    score += evalHangingPiecePenalty(b.bishops_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_MINOR_PENALTY);
    score += evalHangingPiecePenalty(b.rooks_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_ROOK_PENALTY);
    score += evalHangingPiecePenalty(b.queens_bb[side], enemyAttacks, friendlyDef, sign, engine::HANGING_QUEEN_PENALTY);

    return score;
}

int64_t Evaluator::evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept {
    const int64_t scoreWhite = evalHangingPiecesSide(b, data, 0, 1);
    const int64_t scoreBlack = evalHangingPiecesSide(b, data, 1, -1);
    return scoreBlack + scoreWhite;
}

template<uint64_t (*AttackFn)(uint8_t, uint64_t), int64_t PinnedPenalty, int64_t LowMobPenalty>
inline int64_t Evaluator::evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask, int sign) noexcept {
    int64_t score = 0;
    while (piecesBb) {
        const int sq = popLSB(piecesBb);
        const uint64_t attacks = AttackFn(sq, occ);
        const int mobility = __builtin_popcountll(attacks & mobilityMask);
        if (mobility == 0) score -= sign * (PinnedPenalty + TRAPPED_EXTRA_SEVERITY);
        else if (mobility <= 3) score -= sign * LowMobPenalty;
    }
    return score;
}

inline int64_t Evaluator::evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side, int sign) noexcept {
    int64_t sideScore = 0;
    const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                            b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];
    const uint64_t mobilityMask = ~occ;
    const uint64_t mobilityOwnMask = ~ownOcc;

    sideScore += evalTrappedPiecesGeneric<knightAttacksLookup, PINNED_KNIGHT_PENALTY, LOW_MOBILITY_KNIGHT_PENALTY>(
        b.knights_bb[side], occ, mobilityMask, sign);

    if ((b.bishops_bb[side] | b.rooks_bb[side] | b.queens_bb[side]) == 0ULL) {
      return sideScore;
    }

    sideScore += evalTrappedPiecesGeneric<pieces::getBishopAttacks, engine::PINNED_BISHOP_PENALTY, engine::LOW_MOBILITY_BISHOP_PENALTY>(
        b.bishops_bb[side], occ, mobilityOwnMask, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getRookAttacks, engine::PINNED_ROOK_PENALTY, engine::LOW_MOBILITY_ROOK_PENALTY>(
        b.rooks_bb[side], occ, mobilityOwnMask, sign);

    sideScore += evalTrappedPiecesGeneric<pieces::getQueenAttacks, engine::PINNED_QUEEN_PENALTY, engine::LOW_MOBILITY_QUEEN_PENALTY>(
        b.queens_bb[side], occ, mobilityOwnMask, sign);

    return sideScore;
}

int64_t Evaluator::evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept {
    int64_t score = 0;
    for (int side = 0; side < 2; ++side) {
        score += evalTrappedPiecesSide(b, occ, side, (side == 0) ? 1 : -1);
    }
    return score;
}

} // namespace engine
