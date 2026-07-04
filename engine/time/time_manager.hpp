#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace engine::time {

// Parsed UCI `go` search limits. All durations are in milliseconds.
// A value of 0 means "not supplied" unless noted otherwise.
struct Limits {
    int64_t  wtime     = 0;     // White remaining time
    int64_t  btime     = 0;     // Black remaining time
    int64_t  winc      = 0;     // White increment per move
    int64_t  binc      = 0;     // Black increment per move
    int64_t  movetime  = 0;     // Fixed time for this move (0 = not set)
    int64_t  maxDepth  = 0;     // `go depth N` (0 = not set)
    uint64_t maxNodes  = 0;     // `go nodes N` (0 = not set)
    int      movestogo = 0;     // Moves until next time control (0 = sudden death)
    bool     infinite  = false; // `go infinite`
    bool     ponder    = false; // `go ponder`
    bool     hasClock  = false; // true once any of wtime/btime is supplied
};

// Computes a soft/hard time budget for a single move and runs a watchdog
// thread that trips the engine stop flag when the hard limit is reached.
//
// Soft limit  -> checked between iterative-deepening iterations: "do not
//                start a depth I cannot finish".
// Hard limit  -> enforced by the watchdog: aborts the search in progress.
//
// The clock-comparison / "play for flag" machinery from the original design
// is intentionally dropped: the opponent clock barely affects our optimal
// move, and deliberately playing fast trash only raises our own blunder rate.
class TimeManager final {
public:
    using Clock = std::chrono::steady_clock;

    // Communication/GUI lag buffer: never spend the last few ms, so we never
    // flag on the wire even if the GUI reports the clock late.
    static constexpr int64_t MOVE_OVERHEAD_MS   = 30;
    // Absolute floor: always think at least this long when time-managed.
    static constexpr int64_t MIN_THINK_MS       = 5;
    // Fallback horizon when movestogo is absent (sudden death / increment).
    // Floored well above 1 so a single move never grabs a huge share of the
    // clock late in a sudden-death game.
    static constexpr int     DEFAULT_MOVESTOGO  = 50;
    static constexpr int     MIN_MOVESTOGO      = 20;
    // Fraction of the increment we are willing to consume each move.
    static constexpr double  INC_FRACTION       = 0.75;
    // Soft target may never exceed this fraction of remaining time. A single
    // move spending a large slice of the clock is the classic way to flag.
    static constexpr double  OPT_MAX_FRACTION   = 0.20;
    // Hard cap as a fraction of remaining time (true anti-flag ceiling).
    static constexpr double  HARD_MAX_FRACTION  = 0.35;
    // Hard limit as a multiple of the base target (panic ceiling). Kept low:
    // hard ~= base*HARD_MULT, so emergencies cost a small multiple, not 5x.
    static constexpr double  HARD_MULT          = 2.50;
    // Do not start depth d+1 once this fraction of the soft budget is gone.
    static constexpr double  START_NEXT_FRACTION = 0.60;
    // First moves are cheap; ramp the budget in over this many full moves.
    static constexpr int     OPENING_MOVES      = 6;
    // Opening ramp starts here (fraction of base on the very first move).
    static constexpr double  OPENING_MIN_SCALE  = 0.50;
    // Stability multiplier bounds applied to the soft budget.
    static constexpr double  STABILITY_MIN      = 0.50;
    static constexpr double  STABILITY_MAX      = 2.00;

    TimeManager() noexcept = default;
    ~TimeManager() noexcept;

    TimeManager(const TimeManager&)            = delete;
    TimeManager& operator=(const TimeManager&) = delete;
    TimeManager(TimeManager&&)                 = delete;
    TimeManager& operator=(TimeManager&&)      = delete;

    // Compute the budget. `sideIsWhite` selects which clock/increment to use,
    // `movesPlayed` is the number of full moves already played (for the
    // opening ramp + moves-to-go estimate). `stopFlag` is the engine's
    // stopSearchRequested atomic; the watchdog sets it on the hard deadline.
    void init(const Limits& limits, bool sideIsWhite, int movesPlayed,
              std::atomic<bool>* stopFlag) noexcept;

    // Start the clock and (if time-managed) the hard-deadline watchdog.
    void start() noexcept;
    // Cancel the watchdog and join it. Idempotent.
    void stop() noexcept;

    bool    useTimeManagement() const noexcept { return useTm_; }
    int64_t softLimitMs()       const noexcept { return softMs_; }
    int64_t hardLimitMs()       const noexcept { return hardMs_; }
    int64_t elapsedMs()         const noexcept;

    // Called at the top of the ID loop: false => stop, the next depth almost
    // certainly cannot finish within the soft budget.
    bool shouldStartNextDepth() const noexcept;

    // Feedback after each completed root iteration. Re-scales the soft budget:
    //  - best move changed across iterations  -> spend more (instability)
    //  - score dropped vs previous iteration   -> spend more (panic / fail-low)
    //  - decisive eval with a stable best move -> spend less (easy move)
    void updateStability(bool bestMoveChanged, int32_t score,
                          int32_t prevScore, bool hasPrevScore) noexcept;

private:
    void   watchdogLoop() noexcept;
    void   recomputeSoft() noexcept;
    static int estimateMovesToGo(int movesPlayed) noexcept;

    int64_t baseMs_   = 0;   // base target before stability scaling
    int64_t softCapMs_ = 0;  // OPT_MAX_FRACTION * timeLeft
    int64_t softMs_   = 0;   // current soft budget (base * stability, clamped)
    int64_t hardMs_   = 0;   // hard ceiling (fixed at init)
    bool    useTm_    = false;
    double  stabilityFactor_ = 1.0;

    std::atomic<bool>* stopFlag_ = nullptr;
    Clock::time_point  startTp_{};

    std::thread             watchdog_;
    std::mutex              mtx_;
    std::condition_variable cv_;
    bool                    cancel_  = false;
    bool                    running_ = false;
};

} // namespace engine::time
