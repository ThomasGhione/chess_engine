#pragma once

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

    struct MovePickerData {
        MoveList<chess::Board::Move> moves;
        int32_t scores[MAX_MOVES] {};
        int size = 0;
        int currentIndex = 0;
        bool hashMoveIsLegal = false;

        inline bool hasNext() const noexcept {
            return currentIndex < size;
        }

        inline chess::Board::Move nextMove() noexcept {
            if (currentIndex >= size) return chess::Board::Move{};

            int bestIdx = currentIndex;
            int32_t bestScore = scores[currentIndex];
            for (int i = currentIndex + 1; i < size; ++i) {
                if (scores[i] > bestScore) {
                    bestScore = scores[i];
                    bestIdx = i;
                }
            }

            // Swap
            if (bestIdx != currentIndex) {
                const auto tempMove = moves[currentIndex];
                moves[currentIndex] = moves[bestIdx];
                moves[bestIdx] = tempMove;

                const auto tempScore = scores[currentIndex];
                scores[currentIndex] = scores[bestIdx];
                scores[bestIdx] = tempScore;
            }

            return moves[currentIndex++];
        }
    };

    template <typename MoveType>
    static void insertionSort(MoveList<MoveType>& moves, int32_t* scores) noexcept;

    static MovePickerData prepareMovePicker(
        const MoveList<chess::Board::Move>& moves,
        int ply,
        const chess::Board& b,
        bool usIsWhite,
        uint64_t hashKey,
        const int16_t (&history)[2][64][64],
        const chess::Board::Move (&killerMoves)[2][MAX_PLY],
        const uint16_t (&counterMoves)[64][64],
        const int16_t (&captureHistory)[2][64][7][CAPTURE_HISTORY_SLOTS],
        const TranspositionTable* transpositionTable,
        const chess::Board::Move* previousMove = nullptr,
        int32_t orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING) noexcept;

    static MovePickerData sortLegalMoves(
        const MoveList<chess::Board::Move>& moves,
        int ply,
        const chess::Board& b,
        bool usIsWhite,
        uint64_t hashKey,
        const int16_t (&history)[2][64][64],
        const chess::Board::Move (&killerMoves)[2][MAX_PLY],
        const uint16_t (&counterMoves)[64][64],
        const int16_t (&captureHistory)[2][64][7][CAPTURE_HISTORY_SLOTS],
        const TranspositionTable* transpositionTable,
        const chess::Board::Move* previousMove = nullptr,
        bool* outHashMoveIsLegal = nullptr,
        int32_t orderingPenaltySamePawnOpening = ORDERING_PENALTY_SAME_PAWN_OPENING) noexcept;

    static MovePickerData sortTacticalMoves(
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
    struct MoveOrderingContext {
        const chess::Board& b;
        int ply;
        const chess::Board::Move* previousMove;
        int usSide;
        uint8_t oppKingSq;
        uint64_t occ;
        bool usIsWhite;
        bool isEndgameOrdering;
        int fullMoveClock;
        const int16_t (&history)[2][64][64];
        const chess::Board::Move (&killerMoves)[2][MAX_PLY];
        const uint16_t (&counterMoves)[64][64];
        const int16_t (&captureHistory)[2][64][7][CAPTURE_HISTORY_SLOTS];
        int32_t orderingPenaltySamePawnOpening;
    };

    static constexpr bool sameFromTo(const chess::Board::Move& a, const chess::Board::Move& b) noexcept;
    static constexpr bool sameFromTo(const chess::Board::Move& m, uint8_t from, uint8_t to) noexcept;

    static bool givesCheckAfterQuietMoveFast(
        const chess::Board& b,
        const chess::Board::Move& m,
        uint8_t fromPieceType,
        int usSide,
        uint8_t oppKingSq,
        uint64_t occ) noexcept;

    static int32_t staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) noexcept;

    static int32_t scoreMoveOrderingPriorityInline(
        const MoveOrderingContext& ctx,
        const chess::Board::Move& m,
        uint8_t fromPieceType,
        bool isCapture,
        uint8_t victimType,
        int32_t see,
        bool isPromotionCandidate,
        int moveIndex,
        bool isHashMove) noexcept;

    static uint8_t getLeastValuableAttackerTo(
        const chess::Board& b,
        uint8_t sq,
        uint64_t occLocal,
        int sideLocal) noexcept;

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
};

} // namespace engine
