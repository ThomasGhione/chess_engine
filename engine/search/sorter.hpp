#ifndef ENGINE_SEARCH_SORTER_HPP
#define ENGINE_SEARCH_SORTER_HPP

#include <cstdint>

#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../movelist.hpp"

namespace engine {

class Sorter final {
public:
    Sorter() = delete;
    ~Sorter() = delete;

    static constexpr int MAX_PLY = 64;
    static constexpr int CAPTURE_HISTORY_SLOTS = 2;

    static MoveList<chess::Board::Move> sortLegalMoves(
        const MoveList<chess::Board::Move>& moves,
        int ply,
        chess::Board& b,
        bool usIsWhite,
        uint64_t hashKey,
        const int16_t (&history)[2][64][64],
        const chess::Board::Move (&killerMoves)[2][MAX_PLY],
        const uint16_t (&counterMoves)[64][64],
        const int16_t (&captureHistory)[2][64][7][CAPTURE_HISTORY_SLOTS],
        const tt::TranspositionTable* transpositionTable,
        const chess::Board::Move* previousMove = nullptr,
        bool* outHashMoveIsLegal = nullptr,
        int32_t orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING) noexcept;

    static MoveList<chess::Board::Move> sortTacticalMoves(
        const MoveList<chess::Board::Move>& tacticalMoves,
        const chess::Board& b,
        int32_t standPat,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool usIsWhite,
        int32_t searchDepth) noexcept;

    static MoveList<chess::Board::Move> sortEvasionsForcingFirst(
        const MoveList<chess::Board::Move>& evasions,
        const chess::Board& b) noexcept;

private:
    static bool sameFromTo(const chess::Board::Move& a, const chess::Board::Move& b) noexcept;
    static bool sameFromTo(const chess::Board::Move& m, uint8_t from, uint8_t to) noexcept;
    static bool containsMoveWithPromotion(
        const MoveList<chess::Board::Move>& moves,
        uint8_t from,
        uint8_t to,
        char promotionPiece) noexcept;

    static bool givesCheckAfterQuietMoveFast(
        const chess::Board& b,
        const chess::Board::Move& m,
        uint8_t fromPieceType,
        int usSide,
        uint8_t oppKingSq,
        uint64_t occ) noexcept;

    static int32_t clampOrderingScore(int64_t score) noexcept;

    static int32_t scoreMoveOrderingPriorityInline(
        chess::Board& b,
        const chess::Board::Move& m,
        uint8_t fromPieceType,
        bool isCapture,
        uint8_t victimType,
        int32_t see,
        bool isPromotionCandidate,
        int moveIndex,
        bool hashMoveIsLegal,
        uint8_t hashFrom,
        uint8_t hashTo,
        char hashPromo,
        int ply,
        const chess::Board::Move* previousMove,
        int usSide,
        uint8_t oppKingSq,
        uint64_t occ,
        bool usIsWhite,
        bool isEndgameOrdering,
        int fullMoveClock,
        const int16_t (&history)[2][64][64],
        const chess::Board::Move (&killerMoves)[2][MAX_PLY],
        const uint16_t (&counterMoves)[64][64],
        const int16_t (&captureHistory)[2][64][7][CAPTURE_HISTORY_SLOTS],
        const int32_t (&pieceValues)[8],
        int32_t orderingPenaltySamePawnOpening) noexcept;

    static uint8_t getLeastValuableAttackerTo(
        const chess::Board& b,
        uint8_t sq,
        uint64_t occLocal,
        int sideLocal) noexcept;

    static int32_t staticExchangeEvaluation(
        const chess::Board& b,
        const chess::Board::Move& m) noexcept;

    static int32_t clampQMoveScore(int64_t score) noexcept;
    static bool shouldDeltaPrune(
        int32_t standPat,
        int32_t margin,
        int32_t alpha,
        int32_t beta,
        bool isWhite) noexcept;

    static bool isForcingEvasion(
        const chess::Board& b,
        const chess::Board::Move& m,
        const chess::Coords& enPassant,
        bool hasEnPassant) noexcept;

    static bool isPromotionMove(
        const chess::Board& board,
        const chess::Board::Move& move) noexcept;

    static bool doMoveWithPromotion(
        chess::Board& b,
        const chess::Board::Move& m,
        chess::Board::MoveState& state) noexcept;
};

} // namespace engine

#endif // ENGINE_SEARCH_SORTER_HPP
