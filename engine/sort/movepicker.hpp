#pragma once

#include <cstdint>
#include <cstring>

#include "../../board/board.hpp"
#include "../movelist.hpp"
#include "../search/search_constants.hpp"

namespace engine {

// Defined in sorter.cpp; declared here to break the sorter<->movepicker
// circular dependency (finalizeSee needs SEE, which lives in Sorter).
int32_t computeSeeForPicker(const chess::Board& b, const chess::Board::Move& m) noexcept;

// Deferred-SEE state of a move in the picker: its score is either already
// final, or a capture / quiet whose SEE (good-vs-bad split, hanging-quiet
// demotion) is resolved lazily by MovePicker::finalizeSee.
enum class SeePending : uint8_t { Final = 0, Capture = 1, Quiet = 2 };

// Incremental lazy-selection picker. Parallel arrays (moves/scores/seePending)
// are kept in sync by nextMove() and fullSort(); never manipulate them separately.
struct MovePicker {
    MoveList<chess::Board::Move> moves;
    int32_t    scores[MAX_MOVES] {};
    SeePending seePending[MAX_MOVES] {};
    const chess::Board* board = nullptr; // SEE source for deferred finalisation
    int  size         = 0;
    int  currentIndex = 0;
    bool hashMoveIsLegal = false;

    inline bool hasNext() const noexcept { return currentIndex < size; }

    // Resolve a deferred capture/quiet into its final score (good capture kept,
    // losing capture or hanging quiet demoted). Returns true if the score dropped
    // (so the caller must re-pick the max).
    inline bool finalizeSee(int idx) noexcept {
        const SeePending pending = seePending[idx];
        if (pending == SeePending::Final) return false;
        seePending[idx] = SeePending::Final;
        const int32_t see = computeSeeForPicker(*board, moves[idx]);
        if (see >= 0) return false;                      // good capture / safe quiet: unchanged
        if (pending == SeePending::Capture) {            // losing capture
            scores[idx] = -CAPTURE_BASE_SCORE + see;
        } else {                                         // quiet that hangs material
            const int32_t cap = KILLER_2_SCORE - 1;
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
                std::swap(moves[currentIndex],      moves[bestIdx]);
                std::swap(scores[currentIndex],     scores[bestIdx]);
                std::swap(seePending[currentIndex], seePending[bestIdx]);
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
            int j = i - 1;
            while (j >= 0 && scores[j] < keyScore) {
                scores[j + 1] = scores[j];
                moves[j + 1]  = moves[j];
                --j;
            }
            scores[j + 1] = keyScore;
            moves[j + 1]  = keyMove;
        }
    }
};

} // namespace engine
