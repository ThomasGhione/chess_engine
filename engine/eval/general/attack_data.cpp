#include "../evaluator.hpp"

namespace engine {

static constexpr size_t ATTACK_CACHE_SIZE = 1u << 8; // 256 entries (~16 KiB), L1-friendly.
static constexpr uint64_t ATTACK_CACHE_MASK = static_cast<uint64_t>(ATTACK_CACHE_SIZE - 1u);

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
