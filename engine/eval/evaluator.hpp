#ifndef ENGINE_EVAL_EVALUATOR_HPP
#define ENGINE_EVAL_EVALUATOR_HPP

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include "../../board/board.hpp"
#include "../basebonuspenaltyvalues.hpp"

namespace engine {

class Evaluator final {
public:
    Evaluator() = delete;

    static int64_t evaluate(const chess::Board& board) noexcept;
    static int64_t evaluateTrace(const chess::Board& board) noexcept;
    static int64_t evaluateCheckmate(const chess::Board& board) noexcept;
    static int64_t getMaterialDelta(const chess::Board& b) noexcept;

    // Exposed for perf tests (benchmark coverage)
    static int64_t evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static int64_t evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static int64_t evalEndgameKingActivity(const chess::Board& b) noexcept;
    static int64_t evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;

private:
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

    static constexpr int manhattan(int a, int b) noexcept;
    static constexpr uint64_t adjacentFilesMask(int file) noexcept;
    static constexpr std::array<uint64_t, 64> initWhiteForwardFill();
    static constexpr std::array<uint64_t, 64> initBlackForwardFill();

    static void addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int64_t* table, int64_t& eval) noexcept;

    template<bool IsEndgame>
    static constexpr int64_t evalInitiativeImpl(uint8_t activeColor) noexcept;
    static int64_t evalInitiative(const chess::Board& b, bool isEndgame) noexcept;
    template<int Side>
    static constexpr int64_t evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept;

    static void computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;
    static inline void ensureAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;

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
};

inline constexpr int Evaluator::manhattan(int a, int b) noexcept {
    return std::abs((a & 7) - (b & 7)) + std::abs((a >> 3) - (b >> 3));
}

inline constexpr uint64_t Evaluator::adjacentFilesMask(int file) noexcept {
    uint64_t m = 0;
    if (file > 0) m |= chess::Board::fileMask(file - 1);
    if (file < 7) m |= chess::Board::fileMask(file + 1);
    return m;
}

inline constexpr std::array<uint64_t, 64> Evaluator::initWhiteForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::Board::rankOf(sq);
        result[sq] = (rank > 0) ? ((chess::Board::bitMask(rank * 8)) - 1ULL) : 0ULL;
    }
    return result;
}

inline constexpr std::array<uint64_t, 64> Evaluator::initBlackForwardFill() {
    std::array<uint64_t, 64> result{};
    for (int sq = 0; sq < 64; ++sq) {
        const int rank = chess::Board::rankOf(sq);
        result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
    }
    return result;
}

template<bool IsEndgame>
inline constexpr int64_t Evaluator::evalInitiativeImpl(uint8_t activeColor) noexcept {
    constexpr int64_t bonus = IsEndgame ? INIT_BONUS_EG : INIT_BONUS_MG;
    return (activeColor == chess::Board::WHITE) ? bonus : -bonus;
}

template<int Side>
inline constexpr int64_t Evaluator::evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
    static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");

    const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
    const int lightPawnCount = __builtin_popcountll(pawns & LIGHT_SQUARES);

    const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
    const int lightBishops = __builtin_popcountll(bishops & LIGHT_SQUARES);

    const int64_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * 8);

    if constexpr (Side == 0) {
        return score;
    } else {
        return -score;
    }
}

inline constexpr int64_t Evaluator::getPieceValue(uint8_t pieceType) noexcept {
    return PIECE_VALUES[pieceType & chess::Board::MASK_PIECE_TYPE];
}

inline void Evaluator::ensureAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
    if (!data[0].isComputed) {
        computeAttackData(data, b, occ);
    }
}

} // namespace engine

#endif // ENGINE_EVAL_EVALUATOR_HPP
