#include "../evaluator.hpp"

namespace engine {

static constexpr size_t ATTACK_CACHE_SIZE = 1u << 9; // 512 entries (~32 KiB), tests/perf tuned.
static constexpr uint64_t ATTACK_CACHE_MASK = ATTACK_CACHE_SIZE - 1u;

inline void Evaluator::processPawns(uint64_t pawns, AttackData& data, bool isWhite) noexcept {
    data.allAttacks |= collectPawnAttacks(pawns, isWhite ? 0 : 1);
}

template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
inline void Evaluator::processPieces(uint64_t piecesBb, AttackData& data, uint64_t mobilityMask, uint64_t occ,
                                     PhaseValue weight, int32_t ref) noexcept {
    while (piecesBb) {
        const uint64_t attacks = AttackFn(popLSB(piecesBb), occ);
        data.allAttacks |= attacks;
        // Safe mobility: count squares not blocked by our own pieces and not
        // controlled by an enemy pawn (mobilityMask already excludes both).
        const int32_t cnt = std::popcount(attacks & mobilityMask);
        data.mobility += weight * (cnt - ref);
    }
}

inline void Evaluator::computeAttackDataForSide(int side, AttackData& data, const chess::Board& b, uint64_t occ,
                                                uint64_t enemyPawnAttacks) noexcept {
    const uint64_t ownOcc = b.pawns_bb[side] | b.knights_bb[side] | b.bishops_bb[side] |
                            b.rooks_bb[side] | b.queens_bb[side] | b.kings_bb[side];
    // Mobility area excludes our own pieces and squares attacked by enemy pawns.
    const uint64_t mobilityMask = ~ownOcc & ~enemyPawnAttacks;
    const bool isWhite = (side == 0);

    processPawns(b.pawns_bb[side], data, isWhite);
    processPieces<knightAttacksLookup>(b.knights_bb[side], data, mobilityMask, occ, MOBILITY_KNIGHT_WEIGHT, MOBILITY_KNIGHT_REF);
    processPieces<pieces::getBishopAttacks>(b.bishops_bb[side], data, mobilityMask, occ, MOBILITY_BISHOP_WEIGHT, MOBILITY_BISHOP_REF);
    processPieces<pieces::getRookAttacks>(b.rooks_bb[side], data, mobilityMask, occ, MOBILITY_ROOK_WEIGHT, MOBILITY_ROOK_REF);
    processPieces<pieces::getQueenAttacks>(b.queens_bb[side], data, mobilityMask, occ, MOBILITY_QUEEN_WEIGHT, MOBILITY_QUEEN_REF);
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

    // Enemy pawn attacks are needed up front to carve them out of each side's
    // safe-mobility area (side 0's area excludes side 1's pawn attacks, etc.).
    const uint64_t pawnAttacks0 = collectPawnAttacks(b.pawns_bb[0], 0);
    const uint64_t pawnAttacks1 = collectPawnAttacks(b.pawns_bb[1], 1);

    computeAttackDataForSide(0, data[0], b, occ, pawnAttacks1);
    computeAttackDataForSide(1, data[1], b, occ, pawnAttacks0);

    cacheEntry.key = cacheKey;
    cacheEntry.value[0] = data[0];
    cacheEntry.value[1] = data[1];
    cacheEntry.valid = 1;
}

PhaseValue Evaluator::evalMobility(const AttackData data[2]) noexcept {
    // Per-side scores are already (mg, eg) bonuses centred on each piece's ref;
    // white-relative mobility is simply white minus black.
    return data[0].mobility - data[1].mobility;
}

} // namespace engine
