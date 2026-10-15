#pragma once

#include <cstdint>
#include <cstring>
#include <utility>  // for std::move

#include "../../board/board.hpp"
#include "../movelist.hpp"
#include "../search/search_constants.hpp"

namespace engine {

// Defined in sorter.cpp; declared here to break the sorter<->movepicker
// circular dependency (finalizeSee needs SEE, which lives in Sorter).
int32_t computeSeeForPicker(const chess::Board& b, const chess::Move& m) noexcept;

// Deferred-SEE state of a move in the picker: its score is either already
// final, or a capture / quiet whose SEE (good-vs-bad split, hanging-quiet
// demotion) is resolved lazily by MovePicker::finalizeSee.
enum class SeePending : uint8_t { Final = 0, Capture = 1, Quiet = 2 };

// Incremental lazy-selection picker. Parallel arrays (moves/keys/seePending)
// are kept in sync by nextMove() and fullSort(); never manipulate them separately.
//
// Like MoveList, only entries [0, size) are ever live: producers MUST fill both
// keys[i] (via setScore) and seePending[i] for every i < size before the picker is read or
// copied. The arrays are intentionally left uninitialised past `size`, and the
// copy/move ops below touch only [0, size) -- so on the hot quiescence path the
// per-node array traffic is O(size), not O(MAX_MOVES).
struct MovePicker {
    MoveList moves;
    // Selection key per live slot: the ordering score in the high 24 bits and
    // the inverted slot index in the low 8. The largest key is the highest
    // score, ties to the lowest slot, so one plain max finds the pick.
    uint32_t   keys[MAX_MOVES];
    SeePending seePending[MAX_MOVES];
    int  currentIndex = 0;
    const chess::Board* board = nullptr; // SEE source for deferred finalisation

    MovePicker() noexcept = default;
    MovePicker(const MovePicker& o) noexcept { copyFrom(o); }
    MovePicker(MovePicker&& o) noexcept { moveFrom(std::move(o)); }
    MovePicker& operator=(const MovePicker& o) noexcept {
        if (this != &o) copyFrom(o);
        return *this;
    }
    MovePicker& operator=(MovePicker&& o) noexcept {
        if (this != &o) moveFrom(std::move(o));
        return *this;
    }

    inline bool hasNext() const noexcept { return currentIndex < moves.size; }

    static constexpr uint32_t makeKey(int32_t score, int slot) noexcept {
        return (static_cast<uint32_t>(score + KEY_SCORE_BIAS) << 8) | static_cast<uint32_t>(255 - slot);
    }
    inline int32_t score(int slot) const noexcept {
        return static_cast<int32_t>(keys[slot] >> 8) - KEY_SCORE_BIAS;
    }
    inline void setScore(int slot, int32_t s) noexcept { keys[slot] = makeKey(s, slot); }

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
            setScore(idx, -CAPTURE_BASE_SCORE + see);
        } else {                                         // quiet that hangs material
            const int32_t cap = KILLER_2_SCORE - 1;
            const int32_t s = score(idx);
            setScore(idx, (s < cap ? s : cap) + see);
        }
        return true;
    }

    inline chess::Move nextMove() noexcept {
        while (currentIndex < moves.size) {
            uint32_t best = keys[currentIndex];
            for (int i = currentIndex + 1; i < moves.size; ++i) {
                best = best > keys[i] ? best : keys[i];
            }
            const int bestIdx = 255 - static_cast<int>(best & 255u);

            // Deferred SEE: finalise the top candidate; only when it actually
            // demotes itself do we re-pick (a kept move is still the max).
            if (finalizeSee(bestIdx)) {
                continue;
            }

            if (bestIdx != currentIndex) {
                // The move at currentIndex takes the picked one's slot, and
                // with it that slot's tie rank.
                std::swap(moves[currentIndex],      moves[bestIdx]);
                std::swap(seePending[currentIndex], seePending[bestIdx]);
                keys[bestIdx]      = (keys[currentIndex] & ~255u) | static_cast<uint32_t>(255 - bestIdx);
                keys[currentIndex] = (best & ~255u) | static_cast<uint32_t>(255 - currentIndex);
            }

            return moves[currentIndex++];
        }
        return {};
    }

    // Full descending insertion sort over [0, moves.size). Used by the root search.
    inline void fullSort() noexcept {
        for (int i = 0; i < moves.size; ++i) finalizeSee(i); // resolve deferred SEE before sorting
        // Stable on the score alone (the slot bytes go stale; the root only
        // reads the moves afterwards).
        for (int i = 1; i < moves.size; ++i) {
            const auto     keyMove  = moves[i];
            const uint32_t keyValue = keys[i];
            int j = i - 1;
            while (j >= 0 && (keys[j] >> 8) < (keyValue >> 8)) {
                keys[j + 1]  = keys[j];
                moves[j + 1] = moves[j];
                --j;
            }
            keys[j + 1]  = keyValue;
            moves[j + 1] = keyMove;
        }
    }

private:
    // Lifts every reachable score (losing captures sit near -11000, the hash
    // move at 100000) into the unsigned 24-bit field.
    static constexpr int32_t KEY_SCORE_BIAS = 65536;
    static_assert(MAX_MOVES <= 256, "slot indices must fit the key's low byte");

    // Copy/move only the live prefix [0, moves.size); scores and seePending past
    // it are indeterminate and must never be read. Mirrors MoveList's
    // size-bounded copyFrom/moveFrom. `moves` must be assigned before this runs
    // (it is the size authority; o.moves.size is already 0 after a move).
    inline void copyLivePrefix(const MovePicker& o) noexcept {
        board        = o.board;
        currentIndex = o.currentIndex;
        const size_t n = static_cast<size_t>(moves.size);
        std::memcpy(keys,       o.keys,       n * sizeof(keys[0]));
        std::memcpy(seePending, o.seePending, n * sizeof(seePending[0]));
    }

    inline void copyFrom(const MovePicker& o) noexcept {
        moves = o.moves;
        copyLivePrefix(o);
    }

    inline void moveFrom(MovePicker&& o) noexcept {
        moves = std::move(o.moves);
        copyLivePrefix(o);
        o.currentIndex = 0;
    }
};

} // namespace engine
