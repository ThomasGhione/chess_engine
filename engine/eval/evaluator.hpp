#ifndef ENGINE_EVAL_EVALUATOR_HPP
#define ENGINE_EVAL_EVALUATOR_HPP

// Per variabili interne
#include <array>
#include <cstdint>
#include <limits>

#include <cstdlib>
#include <algorithm>
#include <cstring>

#include "../../board/board.hpp"
#include "../basebonuspenaltyvalues.hpp"
#include "../inl/bitboard_helpers_01.inl"

namespace engine {

class Evaluator final {
public:
    // Costruttore
    Evaluator() = delete;
    // Costruttore end

    // Metodi statici
    static int64_t evaluate(const chess::Board& board) noexcept;
    static int64_t evaluateTrace(const chess::Board& board) noexcept;
    static int64_t evaluateCheckmate(const chess::Board& board) noexcept;
    static int64_t getMaterialDelta(const chess::Board& b) noexcept;

    static int64_t evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static int64_t evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static int64_t evalEndgameKingActivity(const chess::Board& b) noexcept;
    static int64_t evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;
    // Metodi statici end

private:
    // Strutture
    struct AttackData {
        uint64_t allAttacks;
        uint64_t pawnAttacks;
        uint64_t knightAttacks;
        uint64_t bishopAttacks;
        uint64_t rookAttacks;
        uint64_t queenAttacks;

        int64_t knightMobility;
        int64_t bishopMobility;
        int64_t rookMobility;
        int64_t queenMobility;

        bool isComputed;
    };
    // Strutture end

    // Variabili
    static constexpr int64_t PIECE_VALUES[8] = {
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

    static constexpr uint64_t DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
    static constexpr uint64_t LIGHT_SQUARES = ~DARK_SQUARES;

    static const std::array<uint64_t, 64> WHITE_FORWARD_FILL;
    static const std::array<uint64_t, 64> BLACK_FORWARD_FILL;

    static const std::array<uint64_t, 8> FILE_MASKS;
    static const std::array<uint64_t, 8> ADJACENT_FILES_ONLY;
    static const std::array<uint64_t, 8> ADJACENT_AND_FILE_MASKS;
    static const std::array<uint64_t, 64> KING_PROXIMITY_MASKS;

    static inline constexpr int64_t NEG_INF = std::numeric_limits<int64_t>::min();
    static inline constexpr int64_t POS_INF = std::numeric_limits<int64_t>::max();
    static inline constexpr int64_t TRAPPED_EXTRA_SEVERITY = 10; // in centipawns
    // Variabili end

    // Metodi
    static int64_t evalInitiative(const chess::Board& b, bool isEndgame) noexcept;
    static constexpr int manhattan(int a, int b) noexcept;
    static constexpr uint64_t adjacentFilesMask(int file) noexcept;

    static void addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int64_t* table, int64_t& eval) noexcept;

    template<bool IsEndgame>
    static constexpr int64_t evalInitiativeImpl(uint8_t activeColor) noexcept;

    template<int Side>
    static constexpr int64_t evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept;

    static void computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;
    static inline void ensureAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;

    static inline uint64_t knightAttacksLookup(uint8_t sq, uint64_t) noexcept;
    template<uint64_t (*AttackFn)(uint8_t, uint64_t), int64_t PinnedPenalty, int64_t LowMobPenalty>
    static inline int64_t evalTrappedPiecesGeneric(uint64_t piecesBb, uint64_t occ, uint64_t mobilityMask, int sign) noexcept;
    static inline int64_t evalTrappedPiecesSide(const chess::Board& b, uint64_t occ, int side, int sign) noexcept;
    static inline int64_t evalHangingPiecesSide(const chess::Board& b, const AttackData data[2], int side, int sign) noexcept;
    template<int64_t Bonus>
    static inline int64_t evalOutpostsPieces(uint64_t piecesBb, int color, int opp, int sign, const chess::Board& b) noexcept;
    template<bool IsEndgame>
    static inline int64_t evalKingActivitySide(const chess::Board& b, int side) noexcept;
    template<int Side>
    static inline int64_t evalEndgameKingActivitySide(const chess::Board& b) noexcept;

    static inline int64_t evalRooksForColor(int color, uint64_t rooks, uint64_t ownPawns, uint64_t oppPawns) noexcept;
    static inline int64_t evalPieceCoordinationForColor(const chess::Board& b, int color) noexcept;
    static inline int64_t evalOutpostsForColor(const chess::Board& b, int color) noexcept;
    static int64_t evalMobility(const AttackData data[2]) noexcept;
    static int64_t evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int64_t evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept;
    static int64_t evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept;
    static int64_t evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int64_t evalMinorPieceDevelopment(const chess::Board& b) noexcept;
    static int64_t evalEarlyQueen(const chess::Board& b) noexcept;
    static int64_t evalPieceCoordination(const chess::Board& b) noexcept;
    static int64_t evalOutposts(const chess::Board& b) noexcept;
    static int64_t evalCastlingBonus(const chess::Board& b) noexcept;
    static int64_t evalBlockedPawnByBishops(const chess::Board& b) noexcept;
    static int64_t evalRookEndgamePressure(const chess::Board& b) noexcept;
    static int64_t evalQueenEndgamePressure(const chess::Board& b) noexcept;
    static int64_t evalDoubleRookEndgame(const chess::Board& b) noexcept;
    static constexpr int64_t getPieceValue(uint8_t pieceType) noexcept;

    static inline uint8_t popLSB(uint64_t& bb) noexcept;
    static inline int64_t evaluateOpeningPhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, const AttackData data[2]) noexcept;
    static inline int64_t evaluateEarlyMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    static inline int64_t evaluateMiddlegamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    static inline int64_t evaluateEndgamePhase(const chess::Board& b, int64_t eval, uint64_t whitePawns, uint64_t blackPawns, uint64_t occ, const AttackData data[2]) noexcept;
    // Metodi end
};

#include "evaluator.inl"

} // namespace engine

#endif // ENGINE_EVAL_EVALUATOR_HPP
