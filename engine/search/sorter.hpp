#pragma once

#include <cstdint>
#include <cstring>

#include "../../board/board.hpp"
#include "../../tt/tt.hpp"
#include "../movelist.hpp"

namespace engine {

// Full definition lives in searcher.hpp; sorter.cpp only needs the members.
struct SearchRuntime;

class Sorter final {
public:
    Sorter() = delete;
    ~Sorter() = delete;

    static constexpr int MAX_PLY = 64;
    static constexpr int CAPTURE_HISTORY_SLOTS = 2;

    // --- Move ordering score thresholds ---
    static constexpr int32_t HASH_MOVE_SCORE      = 100000;
    static constexpr int32_t CAPTURE_BASE_SCORE   = 10000;
    static constexpr int32_t KILLER_1_SCORE       = 9000;
    static constexpr int32_t KILLER_2_SCORE       = 8500;
    static constexpr int32_t COUNTER_MOVE_SCORE   = 8200;
    static constexpr int32_t CHECK_QUIET_SCORE    = 8000;
    static constexpr int32_t PROMOTION_BASE_SCORE = 7000;
    static constexpr int32_t HISTORY_SCORE_MAX    = 7500;
    static constexpr int32_t HISTORY_SCORE_MIN    = -2000;

    // --- Opening king ordering ---
    static constexpr int32_t OPENING_KING_MOVE_PENALTY  = 220;
    static constexpr int32_t CASTLING_BONUS             = 550;
    static constexpr int     OPENING_FULLMOVE_THRESHOLD = 10;

    // --- Qsearch tactical scoring ---
    static constexpr int32_t TACTICAL_PROMOTION_SCORE    = 9000;
    static constexpr int32_t FUTILITY_MARGIN             = 100;
    static constexpr int32_t MOVE_DELTA_MARGIN           = 140;
    static constexpr int32_t SEE_THRESHOLD_SHALLOW       = -24;  // ply < 10
    static constexpr int32_t SEE_THRESHOLD_MID           = -12;  // 10 <= ply < 20
    static constexpr int32_t SEE_THRESHOLD_DEEP          = -4;   // ply >= 20

    // Inlined here so callers (in header-included contexts) get guaranteed inlining.
    // Returns EMPTY (0) as a sentinel for unrecognised input so SEE / move-ordering
    // can detect garbage instead of silently treating it as QUEEN. Legitimate move
    // sources (MoveGenerator, TT decode) only ever produce q/r/b/n.
    static inline constexpr uint8_t promotionPieceType(char promotionPiece) noexcept {
        switch (promotionPiece) {
            case 'q': case 'Q': return chess::Board::QUEEN;
            case 'r': case 'R': return chess::Board::ROOK;
            case 'b': case 'B': return chess::Board::BISHOP;
            case 'n': case 'N': return chess::Board::KNIGHT;
            default:            return chess::Board::EMPTY;
        }
    }

    static inline constexpr int32_t getPromotionValueDelta(char promotionPiece) noexcept {
        const uint8_t pt = promotionPieceType(promotionPiece);
        return (pt == chess::Board::EMPTY) ? 0
                                           : PIECE_VALUES[pt] - PIECE_VALUES[chess::Board::PAWN];
    }

    // Deferred-SEE state of a move in the picker: its score is either already final,
    // or a capture / quiet whose SEE (good-vs-bad split, hanging-quiet demotion) is
    // resolved lazily by MovePickerData::finalizeSee.
    enum class SeePending : uint8_t { Final = 0, Capture = 1, Quiet = 2 };

    // Incremental lazy-selection picker. Parallel arrays (moves/scores/givesCheckFlag)
    // are kept in sync by nextMove() and fullSort(); never manipulate them separately.
    struct MovePickerData {
        MoveList<chess::Board::Move> moves;
        int32_t scores[MAX_MOVES] {};
        // -1 = not computed, 0 = false, 1 = true. Must be -1 for all slots:
        // nextMove() swaps entries including unscored ones, so an uninitialized
        // slot read as 0 would be misinterpreted as "does not give check".
        int8_t givesCheckFlag[MAX_MOVES];
        // Per-move deferred-SEE state (default Final). Keeps SEE off the moves a beta
        // cutoff never examines, while preserving the exact ordering of the eager path.
        SeePending seePending[MAX_MOVES] {};
        const chess::Board* board = nullptr; // SEE source for deferred finalisation
        int  size         = 0;
        int  currentIndex = 0;
        bool hashMoveIsLegal = false;

        MovePickerData() noexcept {
            std::memset(givesCheckFlag, 0xFF, sizeof(givesCheckFlag)); // 0xFF == -1 for int8_t
        }

        inline bool hasNext() const noexcept { return currentIndex < size; }

        // Resolve a deferred capture/quiet into its final score (good capture kept,
        // losing capture or hanging quiet demoted — identical to the old eager path).
        // Returns true if the score dropped (so the caller must re-pick the max).
        inline bool finalizeSee(int idx) noexcept {
            const SeePending pending = seePending[idx];
            if (pending == SeePending::Final) return false;
            seePending[idx] = SeePending::Final;
            const int32_t see = Sorter::staticExchangeEvaluation(*board, moves[idx]);
            if (see >= 0) return false;                      // good capture / safe quiet: unchanged
            if (pending == SeePending::Capture) {            // losing capture
                scores[idx] = -Sorter::CAPTURE_BASE_SCORE + see;
            } else {                                         // quiet that hangs material
                const int32_t cap = Sorter::KILLER_2_SCORE - 1;
                scores[idx] = (scores[idx] < cap ? scores[idx] : cap) + see;
            }
            return true;
        }

        inline chess::Board::Move nextMove() noexcept {
            while (currentIndex < size) {
                int bestIdx = currentIndex;
                int32_t bestScore = scores[currentIndex];
                for (int i = currentIndex + 1; i < size; ++i) {
                    if (scores[i] > bestScore) {
                        bestScore = scores[i];
                        bestIdx = i;
                    }
                }

                // Deferred SEE: finalise the top candidate; only when it actually
                // demotes itself do we re-pick (a kept move is still the max).
                if (finalizeSee(bestIdx)) {
                    continue;
                }

                if (bestIdx != currentIndex) {
                    std::swap(moves[currentIndex],          moves[bestIdx]);
                    std::swap(scores[currentIndex],         scores[bestIdx]);
                    std::swap(givesCheckFlag[currentIndex], givesCheckFlag[bestIdx]);
                    std::swap(seePending[currentIndex],     seePending[bestIdx]);
                }

                return moves[currentIndex++];
            }
            return chess::Board::Move{};
        }

        // Full descending insertion sort over [0, size). Used by root search (YBWC).
        inline void fullSort() noexcept {
            for (int i = 0; i < size; ++i) finalizeSee(i); // resolve deferred SEE before sorting
            for (int i = 1; i < size; ++i) {
                const chess::Board::Move keyMove  = moves[i];
                const int32_t            keyScore = scores[i];
                const int8_t             keyGc    = givesCheckFlag[i];
                int j = i - 1;
                while (j >= 0 && scores[j] < keyScore) {
                    scores[j + 1] = scores[j];
                    moves[j + 1] = moves[j];
                    givesCheckFlag[j + 1] = givesCheckFlag[j];
                    --j;
                }
                scores[j + 1] = keyScore;
                moves[j + 1] = keyMove;
                givesCheckFlag[j + 1] = keyGc;
            }
        }
    };

    static MovePickerData sortLegalMoves(
        MoveList<chess::Board::Move> moves,
        int ply,
        const chess::Board& b,
        const SearchRuntime& runtime,
        const TranspositionTable* transpositionTable,
        const chess::Board::Move* previousMove = nullptr,
        bool* outHashMoveIsLegal = nullptr,
        const int16_t* contHistEntry = nullptr) noexcept;

    struct CaptureInfo {
        bool isCapture;
        bool isEpCapture;
        int  victimType;
    };

    static CaptureInfo classifyCapture(
        const chess::Board::Move& m,
        int fromPieceType,
        int toPieceType,
        const chess::Coords& enPassant) noexcept;

    static int32_t staticExchangeEvaluationPublic(const chess::Board& b, const chess::Board::Move& m) noexcept {
        return staticExchangeEvaluation(b, m);
    }

    static bool givesCheckFast(const chess::Board& b, const chess::Board::Move& m,
            int fromPieceType, int oppKingSq, uint64_t occ) noexcept {
        return givesCheckAfterQuietMoveFast(b, m, fromPieceType, oppKingSq, occ);
    }

    static MovePickerData sortTacticalMoves(
        const MoveList<chess::Board::Move>& tacticalMoves,
        const chess::Board& b,
        int32_t standPat,
        int32_t alpha,
        int ply) noexcept;

    static MoveList<chess::Board::Move> sortEvasionsForcingFirst(
        MoveList<chess::Board::Move> evasions,
        const chess::Board& b) noexcept;

private:
    struct MoveOrderingContext {
        const chess::Board& b;
        int ply;
        const chess::Board::Move* previousMove;
        int     usSide;
        int     oppKingSq;
        uint64_t occ;
        bool    usIsWhite;
        bool    isEndgameOrdering;
        int     fullMoveClock;
        const SearchRuntime& runtime;
        const int16_t* contHistEntry;
    };

    // square == 64 means "no attacker" (type then unspecified).
    struct LeastValuableAttacker {
        int square;
        int type;
    };

    static constexpr bool sameFromTo(const chess::Board::Move& a, const chess::Board::Move& b) noexcept;
    static constexpr bool sameFromTo(const chess::Board::Move& m, int from, int to) noexcept;

    static bool givesCheckAfterQuietMoveFast(
        const chess::Board& b,
        const chess::Board::Move& m,
        int fromPieceType,
        int oppKingSq,
        uint64_t occ) noexcept;

    static int32_t staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) noexcept;

    static int32_t scoreMoveOrderingPriorityInline(
        const MoveOrderingContext& ctx,
        const chess::Board::Move& m,
        bool isCapture,
        int victimType,
        int32_t see,
        bool isPromotionCandidate,
        bool isHashMove,
        int8_t& outGivesCheck) noexcept;

    static LeastValuableAttacker getLeastValuableAttackerTo(
        const chess::Board& b,
        int sq,
        uint64_t occLocal,
        int sideLocal) noexcept;

    static bool isForcingEvasion(
        const chess::Board& b,
        const chess::Board::Move& m,
        const chess::Coords& enPassant) noexcept;
};

} // namespace engine
