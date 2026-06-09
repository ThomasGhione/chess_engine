#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "../../board/board.hpp"
#include "../eval_constants.hpp"
#include "../inl/bitboard_helpers.inl"
#include "phase_value.hpp"

namespace engine {

class Evaluator final {
public:
    Evaluator() = delete;

    // --- Main entry point ---
    static int32_t evaluate(const chess::Board& board) noexcept;
#ifdef DEBUG
    static int32_t evaluateTrace(const chess::Board& board) noexcept;
#endif

    // --- Public eval helpers (used by tests/benchmarks) ---
    // Phase-aware: each returns (mg, eg) so callers can blend with the smooth
    // phase weight. Perf-test sinks may project to `.mg`.
    static PhaseValue evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static PhaseValue evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static PhaseValue evalKingActivityPair(const chess::Board& b) noexcept;
    static PhaseValue evalEndgameKingActivity(const chess::Board& b) noexcept;
    static PhaseValue evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;
    static PhaseValue evalRookEndgamePressureSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept;
    static PhaseValue evalDoubleRookEndgameSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept;

    struct PawnFileStats {
        uint8_t whiteIsolatedFiles = 0;
        uint8_t blackIsolatedFiles = 0;
        int whiteIslands = 0;
        int blackIslands = 0;
        PhaseValue islandScore{};
        PhaseValue doubledScore{};
    };
    static PawnFileStats evalPawnFileStats(uint64_t whitePawns, uint64_t blackPawns) noexcept;

    static inline const std::array<uint64_t, 64>& getPawnSupportMasks(bool isWhite) noexcept;
    static inline const std::array<uint64_t, 64>& getPawnOneStepMasks(bool isWhite) noexcept;

    static bool tryPawnCacheHit(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame, int32_t& outScore) noexcept;
    static void storePawnEvalCache(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame, int32_t score) noexcept;

    static PhaseValue evalPassedPawn(int sq, int rank, uint64_t ownPawns, uint64_t allPawns,
                                      int file, const uint64_t& forwardFill,
                                      const std::array<uint64_t, 64>& oneStepMasks,
                                      const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                      uint64_t enemyPawns,
                                      PhaseValue passedAdvancementScale, PhaseValue passedNearPromotionBonus,
                                      PhaseValue connectedPasserBonus, int promotionRank, int sign) noexcept;

    static PhaseValue evalNonPassedPawn(int rank, uint64_t ownPawns, uint64_t enemyPawns,
                                         uint64_t allPawns, int file, bool hasSupport,
                                         const uint64_t& frontMask, const uint64_t& forwardFill,
                                         uint8_t ownIsolatedFiles,
                                         const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                         PhaseValue candidatePasserBonus, int pawnAttackerIndex,
                                         bool isWhite, int sign) noexcept;

    static PhaseValue evalPawnsByColor(uint64_t ownPawns, uint64_t enemyPawns, uint64_t allPawns,
                                        uint8_t ownIsolatedFiles,
                                        PhaseValue passedAdvancementScale, PhaseValue passedNearPromotionBonus,
                                        PhaseValue connectedPasserBonus, PhaseValue candidatePasserBonus,
                                        int sign) noexcept;

private:
    // --- Internal structures ---
    struct AttackData {
        uint64_t allAttacks = 0ULL;
        // int16 reduces cache footprint; mobility values are tightly bounded.
        int16_t knightMobility = 0;
        int16_t bishopMobility = 0;
        int16_t rookMobility  = 0;
        int16_t queenMobility = 0;
    };

    // Continuous phase descriptor. `w1024` is a fixed-point phase weight in
    // [0, 1024], where 1024 = full opening/middlegame material and 0 = bare
    // endgame. `phaseWeight` is the weighted material count (N=B=1, R=2, Q=4)
    // across both sides; `pawnOnlyEndgame` enables the no-AttackData fast path.
    struct PhaseInfo {
        int32_t phaseWeight     = 0;
        int32_t totalPawns      = 0;
        int32_t w1024           = 0;
        bool    pawnOnlyEndgame = false;
    };

    // --- Static data initializers ---
    static constexpr std::array<uint64_t, 64> initWhiteForwardFill();
    static constexpr std::array<uint64_t, 64> initBlackForwardFill();
    static constexpr std::array<uint64_t, 8>  initFileMasks() noexcept;
    static constexpr std::array<uint64_t, 8>  initAdjacentFilesOnly() noexcept;
    static constexpr std::array<uint64_t, 8>  initAdjacentAndFileMasks() noexcept;
    static constexpr std::array<uint64_t, 64> initKingProximityMasks() noexcept;

    // --- Bitboard constants ---
    static constexpr uint64_t DARK_SQUARES  = 0xAA55AA55AA55AA55ULL;
    static constexpr uint64_t LIGHT_SQUARES = ~DARK_SQUARES;

    static constexpr std::array<uint64_t, 8> RANK_BELOW_MASKS = []() constexpr {
        std::array<uint64_t, 8> t{};
        for (int r = 0; r < 8; ++r)
            t[r] = (r == 0) ? 0ULL : ((1ULL << (r * 8)) - 1ULL);
        return t;
    }();

    static constexpr std::array<uint64_t, 8> RANK_ABOVE_MASKS = []() constexpr {
        std::array<uint64_t, 8> t{};
        for (int r = 0; r < 8; ++r)
            t[r] = (r == 7) ? 0ULL : (~0ULL << ((r + 1) * 8));
        return t;
    }();

    static const std::array<uint64_t, 64> WHITE_FORWARD_FILL;
    static const std::array<uint64_t, 64> BLACK_FORWARD_FILL;
    static const std::array<uint64_t, 8>  FILE_MASKS;
    static const std::array<uint64_t, 8>  ADJACENT_FILES_ONLY;
    static const std::array<uint64_t, 8>  ADJACENT_AND_FILE_MASKS;
    static const std::array<uint64_t, 64> KING_PROXIMITY_MASKS;

    // --- Score limits ---
    // Negamax-safe: NEG_INF == -POS_INF (negating int32_min is UB).
    static inline constexpr int32_t POS_INF                  = std::numeric_limits<int32_t>::max();
    static inline constexpr int32_t NEG_INF                  = -POS_INF;
    static inline constexpr int32_t TRAPPED_EXTRA_SEVERITY   = 10;

    // --- Utilities ---
    static constexpr int manhattan(int a, int b) noexcept;
    static constexpr int chebyshev(int a, int b) noexcept;
    static constexpr int edgeProximity(int sq) noexcept;
    static inline int    ownKingProximity(uint64_t ourKingBB, int enemyKingSq) noexcept;

    // --- Phase detection & orchestration ---
    static inline PhaseInfo classifyPhase(const chess::Board& b) noexcept;

    __attribute__((noinline)) static int32_t evaluateUnifiedPhase(const chess::Board& b, int32_t materialMg, int32_t materialEg, int32_t psqtMg, int32_t psqtEg, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2], int32_t w1024) noexcept;
    __attribute__((noinline)) static int32_t evaluatePawnOnlyEndgamePhase(const chess::Board& b, int32_t materialAndEgPsqt, uint64_t whitePawns, uint64_t blackPawns) noexcept;

    // --- Attack data ---
    __attribute__((noinline))
    static void computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;
    static inline void computeAttackDataForSide(int side, AttackData& data, const chess::Board& b, uint64_t occ) noexcept;
    static inline void processPawns(uint64_t pawns, AttackData& data, bool isWhite) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t), int16_t Evaluator::AttackData::* MobilityField>
    static inline void processPieces(uint64_t piecesBb, AttackData& data, uint64_t mobilityMask, uint64_t occ) noexcept;
    static inline uint64_t knightAttacksLookup(uint8_t sq, uint64_t) noexcept;
    static uint64_t collectPawnAttacks(uint64_t pawns, int side) noexcept;
    static uint64_t collectPawnPushAttacks(uint64_t pawns, int side, uint64_t occ) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
    static uint64_t collectPieceAttacks(uint64_t piecesBb, uint64_t occ) noexcept;

    // --- King safety ---
    static PhaseValue evalKingSafetyWithAttackData(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept;
    static PhaseValue evalKingMiddlegame(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept;
    static int32_t evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept;
    static int32_t evalKingAttackZoneSide(const chess::Board& b, const AttackData data[2], int side, uint64_t occ, int32_t materialScale) noexcept;
    static inline PhaseValue evalKingSafetySide(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2],
                                                  bool whiteCastleKs, bool whiteCastleQs, bool blackCastleKs, bool blackCastleQs,
                                                  int side, int32_t materialScale) noexcept;
    static int32_t attackMaterialScalePercent(const chess::Board& b, int attackingSide, int targetKingFile, uint64_t targetPawns) noexcept;
    static inline int32_t scaleKingDanger(int32_t value, int32_t scalePercent) noexcept;
    static inline void applyNonCastledPenalties(int side, bool rightsLost, bool kingOnWing,
                                                bool canCastleKingside, bool canCastleQueenside,
                                                uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept;
    static inline void applyKingShieldSupport(int side, int sq, uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept;
    static inline void applyHookPawnPenalty(int side, int kingFile, uint64_t ownPawns,
                                            uint64_t ownAttacks, uint64_t enemyAttacks, int32_t& sideSafety) noexcept;
    static inline void applyShelterAndStorm(int side, int kingFile, int kingRank,
                                            uint64_t ownPawns, uint64_t enemyPawns, bool kingOnWing,
                                            uint64_t enemyHeavyPieces, int32_t& sideSafety) noexcept;
    static inline void applyOpenDiagonalPenalty(const chess::Board& b, int kingFile, int kingRank, uint8_t sideColor, int32_t& sideSafety) noexcept;
    static inline void accumulateKingZoneAttackersAll(const chess::Board& b, int side, uint64_t kingZone, uint64_t occ,
                                                      uint64_t developedKnights, uint64_t developedBishops,
                                                      int& attackerCount, int32_t& attackWeight) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
    static inline void accumulateKingZoneAttackers(uint64_t piecesBb, uint64_t kingZone, uint64_t occ,
                                                   int32_t weight, int& attackerCount, int32_t& attackWeight) noexcept;
    static inline void addAllKingCheckUnits(const chess::Board& b, int side, int enemyKingSq,
                                            uint64_t defenderMap, uint64_t occ, int32_t& attackUnits) noexcept;
    static inline void addKingCheckUnits(uint64_t checkers, uint64_t defenderMap,
                                         int32_t safeBonus, int32_t forcingBonus, int32_t& attackUnits) noexcept;

    // --- King activity ---
    template<bool IsEndgame>
    static inline PhaseValue evalKingActivitySide(const chess::Board& b, int side) noexcept;
    template<int Side>
    static inline PhaseValue evalEndgameKingActivitySide(const chess::Board& b) noexcept;
    static inline PhaseValue evalCastlingBonusSide(const chess::Board& b, int side) noexcept;
    static PhaseValue evalMopUp(const chess::Board& b) noexcept;
    static int32_t applyOppColorBishopScaling(const chess::Board& b, int32_t score) noexcept;

    // --- Mobility, threats, trapped pieces ---
    static PhaseValue evalMobility(const AttackData data[2]) noexcept;
    static PhaseValue evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept;
    static inline PhaseValue evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept;
    static inline PhaseValue evalHangingPiecePenalty(uint64_t pieces, uint64_t enemyAttacks, uint64_t friendlyDef,
                                                      int sign, PhaseValue penalty) noexcept;
    static PhaseValue evalThreats(const chess::Board& b, const AttackData data[2], uint64_t occ, bool isEndgame) noexcept;
    static PhaseValue evalThreatsPair(const chess::Board& b, const AttackData data[2], uint64_t occ) noexcept;
    static inline PhaseValue evalThreatsSide(const chess::Board& b, const AttackData data[2], int side, int sign, uint64_t occ) noexcept;
    static PhaseValue evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept;
    static inline PhaseValue evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side, int sign) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
    static inline PhaseValue evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask,
                                                       int sign, PhaseValue pinnedPenalty, PhaseValue lowMobPenalty) noexcept;

    // --- Pawns & space ---
    static PhaseValue evalPawnForks(const chess::Board& b) noexcept;
    static PhaseValue evalSpaceAdvantage(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalInitiative(const chess::Board& b, bool isEndgame) noexcept;
    static PhaseValue evalInitiativePair(const chess::Board& b) noexcept;

    // --- Piece activity ---
    static PhaseValue evalPieceCoordination(const chess::Board& b) noexcept;
    static inline PhaseValue evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept;
    static PhaseValue evalOutposts(const chess::Board& b) noexcept;
    static inline PhaseValue evalOutpostsForColor(const chess::Board& b, int color) noexcept;
    static inline PhaseValue evalOutpostsPieces(uint64_t piecesBb, int color, int opp, int sign,
                                                 const chess::Board& b, PhaseValue bonus) noexcept;
    static inline PhaseValue evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept;
    template<int Side>
    static constexpr PhaseValue evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept;

    // --- Opening & development ---
    static PhaseValue evalMinorPieceDevelopment(const chess::Board& b) noexcept;
    static PhaseValue evalEarlyQueen(const chess::Board& b) noexcept;
    static PhaseValue evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept;
    static PhaseValue evalBlockedPawnByBishops(const chess::Board& b) noexcept;
    static inline PhaseValue evalBlockedPawnByBishopsSide(const chess::Board& b, int side, int fullMoves) noexcept;
    static inline PhaseValue evalBlockedPawnByBishopsPawn(const chess::Board& b, int side, uint64_t bishops, int fullMoves, int psq) noexcept;
    static inline PhaseValue evalCentralBlockPenalty(uint8_t blockerType, int fullMoves) noexcept;
    static PhaseValue evalCastlingBonus(const chess::Board& b) noexcept;

    // --- Endgame ---
    static PhaseValue evalWeakSquares(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalBishopVsKnight(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalPassedPawnKeySquares(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalRuleOfSquare(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalRookEndgamePressure(const chess::Board& b) noexcept;
    static PhaseValue evalQueenEndgamePressure(const chess::Board& b) noexcept;
    static inline PhaseValue evalQueenEndgamePressureSide(const chess::Board& b, int side, int ourQueens, int oppQueens) noexcept;
    static PhaseValue evalDoubleRookEndgame(const chess::Board& b) noexcept;

    // --- Eval cache layer ---
    template<uint32_t Term, class Compute>
    __attribute__((always_inline))
    static inline PhaseValue cachedTerm(const chess::Board& b, Compute compute) noexcept;
    static PhaseValue evalPawnStructureCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept;
    static PhaseValue evalPawnStructureCachedPair(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalBishopPairBonusCached(const chess::Board& b) noexcept;
    static PhaseValue evalCastlingBonusCached(const chess::Board& b) noexcept;
    static PhaseValue evalRooksCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalBadBishopCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalMinorPieceDevelopmentCached(const chess::Board& b) noexcept;
    static PhaseValue evalEarlyQueenCached(const chess::Board& b) noexcept;
    static PhaseValue evalOutpostsCached(const chess::Board& b) noexcept;
    static PhaseValue evalPieceCoordinationCached(const chess::Board& b) noexcept;
    static PhaseValue evalCentralControlCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalWeakSquaresCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static PhaseValue evalBishopVsKnightCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;

#ifdef DEBUG
    static void traceTerm(int32_t& eval, int32_t delta, const char* label) noexcept;
#endif
};

#include "evaluator.inl"

} // namespace engine

#ifdef DEBUG
#include "../../debug.hpp"
#endif
