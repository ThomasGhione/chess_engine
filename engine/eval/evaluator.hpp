#ifndef ENGINE_EVAL_EVALUATOR_HPP
#define ENGINE_EVAL_EVALUATOR_HPP

#include <array>
#include <cstdint>
#include <limits>

#include <cstdlib>
#include <algorithm>
#include <cstring>

#include "../../board/board.hpp"
#include "../basebonuspenaltyvalues.hpp"
#include "../inl/bitboard_helpers.inl"

namespace engine {

class Evaluator final {
public:
    // Constructor
    Evaluator() = delete;
    // Constructor end

    // Static methods
    static int32_t evaluate(const chess::Board& board) noexcept;

    static int32_t evaluateTrace(const chess::Board& board) noexcept;
    
    static int32_t evaluateCheckmate(const chess::Board& board) noexcept;
    static int32_t getMaterialDelta(const chess::Board& b) noexcept;

    static int32_t evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static int32_t evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static int32_t evalEndgameKingActivity(const chess::Board& b) noexcept;
    static int32_t evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;
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
    static constexpr int32_t PIECE_VALUES[8] = {
        0,
        PAWN_VALUE,
        KNIGHT_VALUE,
        BISHOP_VALUE,
        ROOK_VALUE,
        QUEEN_VALUE,
        KING_VALUE,
        0
    };

    static constexpr std::array<uint64_t, 64> initWhiteForwardFill();
    static constexpr std::array<uint64_t, 64> initBlackForwardFill();
    static constexpr std::array<uint64_t, 8> initFileMasks() noexcept;
    static constexpr std::array<uint64_t, 8> initAdjacentFilesOnly() noexcept;
    static constexpr std::array<uint64_t, 8> initAdjacentAndFileMasks() noexcept;
    static constexpr std::array<uint64_t, 64> initKingProximityMasks() noexcept;

    static constexpr uint64_t DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
    static constexpr uint64_t LIGHT_SQUARES = ~DARK_SQUARES;

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
    static constexpr uint64_t adjacentFilesMask(int file) noexcept;

    template<bool IsEndgame>
    static constexpr int32_t evalInitiativeImpl(uint8_t activeColor) noexcept;

    template<int Side>
    static constexpr int32_t evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept;

    __attribute__((noinline))
    static void computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;
    static inline void ensureAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;
    static inline PhaseInfo classifyPhase(const chess::Board& b) noexcept;
    static int32_t evalKingSafetyWithAttackData(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept;

    static inline uint64_t knightAttacksLookup(uint8_t sq, uint64_t) noexcept;
    static inline void addKingCheckUnits(uint64_t checkers, uint64_t defenderMap,
                                         int32_t safeBonus, int32_t forcingBonus,
                                         int32_t& attackUnits) noexcept;
    static inline bool isWhitePassedPawn(int pawnSq, int pawnFile, uint64_t blackPawns) noexcept;
    static inline bool isBlackPassedPawn(int pawnSq, int pawnFile, uint64_t whitePawns) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t), int32_t Weight>
    static inline void accumulateKingZoneAttackers(uint64_t piecesBb, uint64_t kingZone, uint64_t occ,
                                                   int& attackerCount, int32_t& attackWeight) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t), int32_t PinnedPenalty, int32_t LowMobPenalty>
    static inline int32_t evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask, int sign) noexcept;
    static inline int32_t evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side, int sign) noexcept;
    static inline int32_t evalHangingPiecePenalty(uint64_t pieces, uint64_t enemyAttacks, uint64_t friendlyDef,
                                                  int sign, int penalty) noexcept;
    static inline int32_t evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept;
    template<int32_t Bonus>
    static inline int32_t evalOutpostsPieces(uint64_t piecesBb, int color, int opp, int sign, const chess::Board& b) noexcept;
    template<bool IsEndgame>
    static inline int32_t evalKingActivitySide(const chess::Board& b, int side) noexcept;
    template<int Side>
    static inline int32_t evalEndgameKingActivitySide(const chess::Board& b) noexcept;

    static inline int32_t evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept;
    static inline int32_t evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept;
    static inline int32_t evalOutpostsForColor(const chess::Board& b, int color) noexcept;
    static int32_t evalMobility(const AttackData data[2]) noexcept;
    static int32_t evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int32_t evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept;
    static int32_t evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept;
    static int32_t evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int32_t evalMinorPieceDevelopment(const chess::Board& b) noexcept;
    static int32_t evalEarlyQueen(const chess::Board& b) noexcept;
    static int32_t evalPieceCoordination(const chess::Board& b) noexcept;
    static int32_t evalOutposts(const chess::Board& b) noexcept;
    static int32_t evalCastlingBonus(const chess::Board& b) noexcept;
    static int32_t evalBlockedPawnByBishops(const chess::Board& b) noexcept;
    static int32_t evalRookEndgamePressure(const chess::Board& b) noexcept;
    static int32_t evalQueenEndgamePressure(const chess::Board& b) noexcept;
    static int32_t evalDoubleRookEndgame(const chess::Board& b) noexcept;
    static inline uint8_t popLSB(uint64_t& bb) noexcept;
    static void traceTerm(int32_t& eval, int32_t delta, const char* label) noexcept;

    static int32_t getMaterialDeltaCached(const chess::Board& b) noexcept;
    static int32_t evalPawnStructureCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns, bool isEndgame) noexcept;
    static int32_t evalBishopPairBonusCached(const chess::Board& b) noexcept;
    static int32_t evalCastlingBonusCached(const chess::Board& b) noexcept;
    static int32_t evalRooksCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalBadBishopCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int32_t evalBlockedPawnByBishopsCached(const chess::Board& b) noexcept;
    static int32_t evalMinorPieceDevelopmentCached(const chess::Board& b) noexcept;
    static int32_t evalEarlyQueenCached(const chess::Board& b) noexcept;
    static int32_t evalOutpostsCached(const chess::Board& b) noexcept;
    static int32_t evalPieceCoordinationCached(const chess::Board& b) noexcept;
    static int32_t evalCentralControlCached(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;

    __attribute__((noinline))
    static int32_t evaluateOpeningPhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept;
    __attribute__((noinline))
    static int32_t evaluateEarlyMiddlegamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    __attribute__((noinline))
    static int32_t evaluateMiddlegamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    __attribute__((noinline))
    static int32_t evaluateEndgamePhase(const chess::Board& b, int32_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    // Methods end
};

#include "evaluator.inl"

} // namespace engine

#endif // ENGINE_EVAL_EVALUATOR_HPP
