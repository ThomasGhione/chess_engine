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

namespace engine {

class Evaluator final {
public:
    // Constructor
    Evaluator() = delete;
    // Constructor end

    // Static methods
    static int32_t evaluate(const chess::Board& board) noexcept;
#ifdef DEBUG
    static int32_t evaluateTrace(const chess::Board& board) noexcept;
#endif
    static int32_t evaluateCheckmate(const chess::Board& board) noexcept;

    static int32_t evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static int32_t evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static int32_t evalEndgameKingActivity(const chess::Board& b) noexcept;
    static int32_t evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;
    static int32_t evalRookEndgamePressureSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept;
    static int32_t evalDoubleRookEndgameSide(const chess::Board& b, int side, int whiteRooks, int blackRooks) noexcept;
    
    struct PawnFileStats {
        uint8_t whiteIsolatedFiles = 0;
        uint8_t blackIsolatedFiles = 0;
        int whiteIslands = 0;
        int blackIslands = 0;
        int32_t islandScore = 0;
        int32_t doubledScore = 0;
    };

    static PawnFileStats evalPawnFileStats(uint64_t whitePawns, uint64_t blackPawns) noexcept;

    static inline const std::array<uint64_t, 64>& getPawnSupportMasks(bool isWhite) noexcept;
    static inline const std::array<uint64_t, 64>& getPawnOneStepMasks(bool isWhite) noexcept;

    static bool tryPawnCacheHit(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame,
                               int32_t& outScore) noexcept;
    static void storePawnEvalCache(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame,
                                  int32_t score) noexcept;

    static int32_t evalPassedPawn(int sq, int rank, uint64_t ownPawns, uint64_t allPawns,
                                  int file, const uint64_t& forwardFill,
                                  const std::array<uint64_t, 64>& oneStepMasks,
                                  const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                  uint64_t enemyPawns,
                                  int32_t passedAdvancementScale, int32_t passedNearPromotionBonus,
                                  int32_t connectedPasserBonus, int promotionRank, int sign) noexcept;

    static int32_t evalNonPassedPawn(int rank, uint64_t ownPawns, uint64_t enemyPawns,
                                     uint64_t allPawns, int file, bool hasSupport,
                                     const uint64_t& frontMask, const uint64_t& forwardFill,
                                     uint8_t ownIsolatedFiles,
                                     const std::array<uint64_t, 8>& ADJACENT_FILES_ONLY,
                                     int32_t candidatePasserBonus, int pawnAttackerIndex,
                                     bool isWhite, int sign) noexcept;

    static int32_t evalPawnsByColor(uint64_t ownPawns, uint64_t enemyPawns, uint64_t allPawns,
                                    uint8_t ownIsolatedFiles,
                                    int32_t passedAdvancementScale, int32_t passedNearPromotionBonus,
                                    int32_t connectedPasserBonus, int32_t candidatePasserBonus,
                                    int sign) noexcept;
    // Static methods end

private:
    // Structures
    struct AttackData {
        uint64_t allAttacks = 0ULL;
        // Mobility counters are tightly bounded; int16 reduces cache footprint.
        int16_t knightMobility = 0;
        int16_t bishopMobility = 0;
        int16_t rookMobility = 0;
        int16_t queenMobility = 0;
    };
    
    struct PhaseInfo {
        int fullMoves = 0;
        int nonPawnMajors = 0;
        bool isEndgame = false;
        bool isOpening = false;
        bool isEarlyMiddlegame = false;
    };
    // Structures end

    // Variables
    static constexpr std::array<uint64_t, 64> initWhiteForwardFill();
    static constexpr std::array<uint64_t, 64> initBlackForwardFill();
    static constexpr std::array<uint64_t, 8> initFileMasks() noexcept;
    static constexpr std::array<uint64_t, 8> initAdjacentFilesOnly() noexcept;
    static constexpr std::array<uint64_t, 8> initAdjacentAndFileMasks() noexcept;
    static constexpr std::array<uint64_t, 64> initKingProximityMasks() noexcept;

    static constexpr uint64_t DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
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

    static const std::array<uint64_t, 8> FILE_MASKS;
    static const std::array<uint64_t, 8> ADJACENT_FILES_ONLY;
    static const std::array<uint64_t, 8> ADJACENT_AND_FILE_MASKS;
    static const std::array<uint64_t, 64> KING_PROXIMITY_MASKS;
    static inline constexpr int OPENING_MOVES = 8;
    static inline constexpr int EARLY_MG_MOVES = 15;
    static inline constexpr int PIECE_ENDGAME_THRESHOLD = 5;

    // Full int32 range for score bounds/mate scores.
    static inline constexpr int32_t NEG_INF = std::numeric_limits<int32_t>::min();
    static inline constexpr int32_t POS_INF = std::numeric_limits<int32_t>::max();
    static inline constexpr int32_t TRAPPED_EXTRA_SEVERITY = 10; // in centipawns
    // Variables end

    // Methods
    static int32_t evalInitiative(const chess::Board& b, bool isEndgame) noexcept;
    static constexpr int manhattan(int a, int b) noexcept;
    static constexpr int chebyshev(int a, int b) noexcept;
    static constexpr int edgeProximity(int sq) noexcept;
    static inline int ownKingProximity(uint64_t ourKingBB, int enemyKingSq) noexcept;

    template<bool IsEndgame>
    static int32_t evalInitiativeImpl(uint8_t activeColor) noexcept;

    template<int Side>
    static constexpr int32_t evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept;

    __attribute__((noinline))
    static void computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;
    static inline void computeAttackDataForSide(int side, AttackData& data, const chess::Board& b, uint64_t occ) noexcept;
    static inline void processPawns(uint64_t pawns, AttackData& data, bool isWhite) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t), int16_t Evaluator::AttackData::* MobilityField>
    static inline void processPieces(uint64_t piecesBb, AttackData& data, uint64_t mobilityMask, uint64_t occ) noexcept;
    static inline PhaseInfo classifyPhase(const chess::Board& b) noexcept;
    static int32_t evalKingSafetyWithAttackData(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept;

    static inline uint64_t knightAttacksLookup(uint8_t sq, uint64_t) noexcept;
    static inline void addKingCheckUnits(uint64_t checkers, uint64_t defenderMap,
                                         int32_t safeBonus, int32_t forcingBonus,
                                         int32_t& attackUnits) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
    static inline void accumulateKingZoneAttackers(uint64_t piecesBb, uint64_t kingZone, uint64_t occ,
                                                   int32_t weight, int& attackerCount, int32_t& attackWeight) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
    static inline int32_t evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask,
                                                   int sign, int32_t pinnedPenalty, int32_t lowMobPenalty) noexcept;
    static inline int32_t evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side, int sign) noexcept;
    static inline int32_t evalHangingPiecePenalty(uint64_t pieces, uint64_t enemyAttacks, uint64_t friendlyDef,
                                                  int sign, int penalty) noexcept;
    static inline int32_t evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept;
    static inline uint64_t collectPawnAttacks(uint64_t pawns, int side) noexcept;
    static inline uint64_t collectPawnPushAttacks(uint64_t pawns, int side, uint64_t occ) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t)>
    static inline uint64_t collectPieceAttacks(uint64_t piecesBb, uint64_t occ) noexcept;
    static inline int32_t evalThreatsSide(const chess::Board& b, const AttackData data[2], int side,
                                          int sign, uint64_t occ) noexcept;
    static inline int32_t evalOutpostsPieces(uint64_t piecesBb, int color, int opp, int sign,
                                             const chess::Board& b, int32_t bonus) noexcept;
    template<bool IsEndgame>
    static inline int32_t evalKingActivitySide(const chess::Board& b, int side) noexcept;
    template<int Side>
    static inline int32_t evalEndgameKingActivitySide(const chess::Board& b) noexcept;
    static inline void accumulateKingZoneAttackersAll(const chess::Board& b, int side, uint64_t kingZone, uint64_t occ,
                                                      uint64_t developedKnights, uint64_t developedBishops,
                                                      int& attackerCount, int32_t& attackWeight) noexcept;
    static inline int32_t evalKingAttackZoneSide(const chess::Board& b, const AttackData data[2], int side, uint64_t occ) noexcept;
    static inline void addAllKingCheckUnits(const chess::Board& b, int side, int enemyKingSq, uint64_t defenderMap, uint64_t occ, int32_t& attackUnits) noexcept;
    static inline int32_t evalKingSafetySide(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2],
                                             bool whiteCastleKs, bool whiteCastleQs, bool blackCastleKs, bool blackCastleQs, int side) noexcept;
    static int32_t attackMaterialScalePercent(const chess::Board& b, int attackingSide, int targetKingFile,
                                              uint64_t targetPawns) noexcept;
    static inline int32_t scaleKingDanger(int32_t value, int32_t scalePercent) noexcept;
    static inline void applyNonCastledPenalties(int side, bool rightsLost, bool kingOnWing,
                                                bool canCastleKingside, bool canCastleQueenside,
                                                uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept;
    static inline void applyKingShieldSupport(int side, int sq, uint64_t whitePawns, uint64_t blackPawns, int32_t& sideSafety) noexcept;
    static inline void applyHookPawnPenalty(int side, int kingFile, uint64_t ownPawns,
                                            uint64_t ownAttacks, uint64_t enemyAttacks, int32_t& sideSafety) noexcept;
    static inline void applyShelterAndStorm(int side, int kingFile, int kingRank,
                                            uint64_t ownPawns, uint64_t enemyPawns, bool kingOnWing,
                                            const uint64_t enemyHeavyPieces, int32_t& sideSafety) noexcept;
    static inline void applyOpenDiagonalPenalty(const chess::Board& b, int kingFile, int kingRank, uint8_t sideColor, int32_t& sideSafety) noexcept;

    static inline int32_t evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept;
    static inline int32_t evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept;
    static inline int32_t evalOutpostsForColor(const chess::Board& b, int color) noexcept;
    static int32_t evalMobility(const AttackData data[2]) noexcept;
    static int32_t evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int32_t evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept;
    static int32_t evalThreats(const chess::Board& b, const AttackData data[2], uint64_t occ, bool isEndgame) noexcept;
    static int32_t evalPawnForks(const chess::Board& b) noexcept;
    static int32_t evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept;
    static int32_t evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int32_t evalMinorPieceDevelopment(const chess::Board& b) noexcept;
    static int32_t evalEarlyQueen(const chess::Board& b) noexcept;
    static int32_t evalPieceCoordination(const chess::Board& b) noexcept;
    static int32_t evalOutposts(const chess::Board& b) noexcept;
    static int32_t evalCastlingBonus(const chess::Board& b) noexcept;
    static inline int32_t evalCastlingBonusSide(const chess::Board& b, int side) noexcept;
    static int32_t evalBlockedPawnByBishops(const chess::Board& b) noexcept;
    static int32_t evalRookEndgamePressure(const chess::Board& b) noexcept;
    static int32_t evalQueenEndgamePressure(const chess::Board& b) noexcept;
    static int32_t evalMopUp(const chess::Board& b) noexcept;
    static int32_t applyOppColorBishopScaling(const chess::Board& b, int32_t score) noexcept;
    static int32_t evalWeakSquares(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalBishopVsKnight(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalPassedPawnKeySquares(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalSpaceAdvantage(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalRuleOfSquare(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static inline int32_t evalQueenEndgamePressureSide(const chess::Board& b, int side, int ourQueens, int oppQueens) noexcept;
    static int32_t evalDoubleRookEndgame(const chess::Board& b) noexcept;
    static inline int32_t evalCentralBlockPenalty(uint8_t blockerType, int fullMoves) noexcept;
    static inline int32_t evalBlockedPawnByBishopsSide(const chess::Board& b, int side, int fullMoves) noexcept;
    static inline int32_t evalBlockedPawnByBishopsPawn(const chess::Board& b, int side, uint64_t bishops, int fullMoves, int psq) noexcept;
#ifdef DEBUG
    static void traceTerm(int32_t& eval, int32_t delta, const char* label) noexcept;
#endif
    template<uint32_t Term, class Compute>
    __attribute__((always_inline))
    static inline int32_t cachedTerm(const chess::Board& b, Compute compute) noexcept;
    static int32_t evalPawnStructureCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept;
    static int32_t evalBishopPairBonusCached(const chess::Board& b) noexcept;
    static int32_t evalCastlingBonusCached(const chess::Board& b) noexcept;
    static int32_t evalRooksCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalBadBishopCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalMinorPieceDevelopmentCached(const chess::Board& b) noexcept;
    static int32_t evalEarlyQueenCached(const chess::Board& b) noexcept;
    static int32_t evalOutpostsCached(const chess::Board& b) noexcept;
    static int32_t evalPieceCoordinationCached(const chess::Board& b) noexcept;
    static int32_t evalCentralControlCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalWeakSquaresCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalBlockedPawnByBishopsCached(const chess::Board& b) noexcept;
    static int32_t evalBishopVsKnightCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;

    __attribute__((noinline))
    static int32_t evaluateOpeningPhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept;
    __attribute__((noinline))
    static int32_t evaluateEarlyMiddlegamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    __attribute__((noinline))
    static int32_t evaluateMiddlegamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    __attribute__((noinline))
    static int32_t evaluateEndgamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    __attribute__((noinline))
    static int32_t evaluatePawnOnlyEndgamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    // Methods end
};

#include "evaluator.inl"

} // namespace engine

#ifdef DEBUG
#include "../../debug.hpp"
#endif
