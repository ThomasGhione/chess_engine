#include "../evaluator.hpp"

namespace engine {

static constexpr size_t ATTACK_CACHE_SIZE = 1u << 8; // 256 entries (~16 KiB), L1-friendly.
static constexpr uint64_t ATTACK_CACHE_MASK = static_cast<uint64_t>(ATTACK_CACHE_SIZE - 1u);

inline void Evaluator::processPawns(uint64_t pawns, AttackData& data, bool isWhite) noexcept {
    while (pawns) {
        const int sq = popLSB(pawns);
        data.allAttacks |= pieces::PAWN_ATTACKS[isWhite][sq];
    }
}

inline void Evaluator::processKnights(uint64_t knights, AttackData& data, uint64_t mobilityMask) noexcept {
    while (knights) {
        const int sq = popLSB(knights);
        const uint64_t attacks = pieces::KNIGHT_ATTACKS[sq];
        data.allAttacks |= attacks;
        data.knightMobility += __builtin_popcountll(attacks & mobilityMask);
    }
}

inline void Evaluator::processBishops(uint64_t bishops, AttackData& data, uint64_t mobilityMask, uint64_t occ) noexcept {
    while (bishops) {
        const int sq = popLSB(bishops);
        const uint64_t attacks = pieces::getBishopAttacks(sq, occ);
        data.allAttacks |= attacks;
        data.bishopMobility += __builtin_popcountll(attacks & mobilityMask);
    }
}

inline void Evaluator::processRooks(uint64_t rooks, AttackData& data, uint64_t mobilityMask, uint64_t occ) noexcept {
    while (rooks) {
        const int sq = popLSB(rooks);
        const uint64_t attacks = pieces::getRookAttacks(sq, occ);
        data.allAttacks |= attacks;
        data.rookMobility += __builtin_popcountll(attacks & mobilityMask);
    }
}

inline void Evaluator::processQueens(uint64_t queens, AttackData& data, uint64_t mobilityMask, uint64_t occ) noexcept {
    while (queens) {
        const int sq = popLSB(queens);
        const uint64_t attacks = pieces::getQueenAttacks(sq, occ);
        data.allAttacks |= attacks;
        data.queenMobility += __builtin_popcountll(attacks & mobilityMask);
    }
}

inline void Evaluator::computeAttackDataForSide(int side, AttackData& data, const chess::Board& b, uint64_t occ) noexcept {
    const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                            b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];
    const uint64_t mobilityMask = ~ownOcc;
    const bool isWhite = (side == 0);

    processPawns(b.pawns_bb[side], data, isWhite);
    processKnights(b.knights_bb[side], data, mobilityMask);
    processBishops(b.bishops_bb[side], data, mobilityMask, occ);
    processRooks(b.rooks_bb[side], data, mobilityMask, occ);
    processQueens(b.queens_bb[side], data, mobilityMask, occ);
}

void Evaluator::computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
    struct AttackCacheEntry {
        uint64_t key = std::numeric_limits<uint64_t>::max();
        AttackData value[2]{};
        uint8_t valid = 0;
    };

    thread_local std::array<AttackCacheEntry, ATTACK_CACHE_SIZE> attackCache{};

    const uint64_t cacheKey = b.getHash();
    AttackCacheEntry& cacheEntry = attackCache[(cacheKey * 0x9E3779B97F4A7C15ULL) & ATTACK_CACHE_MASK];
    if (cacheEntry.valid && cacheEntry.key == cacheKey) [[likely]] {
        data[0] = cacheEntry.value[0];
        data[1] = cacheEntry.value[1];
        return;
    }

    data[0] = AttackData{};
    data[1] = AttackData{};

    computeAttackDataForSide(0, data[0], b, occ);
    computeAttackDataForSide(1, data[1], b, occ);

    cacheEntry.key = cacheKey;
    cacheEntry.value[0] = data[0];
    cacheEntry.value[1] = data[1];
    cacheEntry.valid = 1;
}

int32_t Evaluator::evalMobility(const AttackData data[2]) noexcept {
    return (data[0].knightMobility + data[0].bishopMobility + data[0].rookMobility + data[0].queenMobility
          - data[1].knightMobility - data[1].bishopMobility - data[1].rookMobility - data[1].queenMobility) / 2;
}

} // namespace engine
