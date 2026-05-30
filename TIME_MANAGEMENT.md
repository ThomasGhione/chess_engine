# Time Management

This document describes the time-management subsystem as currently
implemented. The engine no longer searches to a fixed depth regardless of
the clock: under a UCI time control it computes a per-move budget and stops
the search when that budget is exhausted.

## Goals

- Spend a sensible share of the clock per move instead of always playing
  near-instantly.
- Never lose on time: a communication/overhead buffer and a hard ceiling
  guarantee a move is always returned with time to spare.
- Spend *more* time when the position is unstable (the best move keeps
  changing, or the score is dropping) and *less* when the move is obvious or
  the game is already decided.
- Stay out of the way of fixed-depth, node-limited, `infinite`, and
  `ponder` searches.

The opponent's clock is intentionally **not** used. It barely affects our
own optimal move, and deliberately playing fast to flag the opponent only
raises our own blunder rate.

## Model

All durations are in milliseconds. `myTime` / `myInc` are the side-to-move's
clock and increment.

```
overhead  = MOVE_OVERHEAD_MS                    (GUI/wire lag buffer)
timeLeft  = max(1, myTime - overhead)
mtg       = movestogo            if provided
            max(MIN_MOVESTOGO, DEFAULT_MOVESTOGO - movesPlayed)   otherwise

base      = timeLeft / mtg + myInc * INC_FRACTION
base     *= openingRamp        (first moves, ONLY when myInc == 0)

softCap   = max(MIN_THINK_MS, timeLeft * OPT_MAX_FRACTION)
baseMs    = clamp(base, MIN_THINK_MS, softCap)

hard      = min( timeLeft * HARD_MAX_FRACTION,  baseMs * HARD_MULT )
hard      = clamp(hard, baseMs, timeLeft)        (fixed for the whole move)

soft      = clamp(baseMs * stabilityFactor, MIN_THINK_MS, softCap)
soft      = min(soft, hard)                      (recomputed on feedback)
```

- **Opening ramp**: applied **only in games without an increment**
  (`myInc == 0`). For the first `OPENING_MOVES` full moves the base is
  scaled by `OPENING_MIN_SCALE + (1 - OPENING_MIN_SCALE) * (movesPlayed /
  OPENING_MOVES)`, so early moves do not burn a clock that only shrinks.
  With an increment the clock is replenished every move and the engine
  (which has no opening book) is not throttled in the opening at all.
- **`movetime` mode**: `soft = hard = max(MIN_THINK_MS, movetime - overhead)`.
  No clock estimation is performed.

### Soft vs hard limit

- **Soft limit** — checked at the top of each iterative-deepening
  iteration. Once at least one depth has completed, the next depth is *not*
  started if `elapsed >= soft * START_NEXT_FRACTION`: it almost certainly
  could not finish, so starting it would only waste the clock.
- **Hard limit** — enforced by a watchdog thread created on `start()`. It
  sleeps until `start + hard` and then sets the engine's
  `stopSearchRequested` flag (already polled cheaply throughout the search).
  `stop()` cancels and joins the watchdog; it is idempotent. The hot search
  path is unchanged — no per-node clock reads.

### Stability scaling

After every completed root iteration the search calls back with whether the
best move changed, the new score, and the previous score. The soft limit is
then recomputed from `baseMs * stabilityFactor`, where `stabilityFactor` is
updated multiplicatively and clamped to `[STABILITY_MIN, STABILITY_MAX]`:

| Signal                                            | Effect             |
|---------------------------------------------------|--------------------|
| Best move changed between iterations              | `* 1.35` (instability — think more) |
| Best move stable                                  | `* 0.90` (converging) |
| Score dropped > 30 cp vs previous iteration       | `* (1 + min(1, drop/200))` (panic / fail-low) |
| `abs(score) > 1500` **and** best move stable      | `* 0.60` (decided position / easy move) |

This single multiplicative factor replaces the original design's
conflicting OR-rule lists, so the criteria can no longer contradict each
other; everything is clamped into `[soft, hard]`.

## Constants

Defined in `engine/time/time_manager.hpp` (easy to tune; a natural next step
is to expose them as UCI spin options like the evaluation constants):

| Constant              | Value | Meaning |
|-----------------------|-------|---------|
| `MOVE_OVERHEAD_MS`    | 30    | Lag buffer; never spend the last 30 ms |
| `MIN_THINK_MS`        | 5     | Absolute floor when time-managed |
| `DEFAULT_MOVESTOGO`   | 50    | Assumed horizon when `movestogo` absent |
| `MIN_MOVESTOGO`       | 20    | Lower bound for the estimated horizon |
| `INC_FRACTION`        | 0.75  | Fraction of the increment consumed per move |
| `OPT_MAX_FRACTION`    | 0.20  | Soft target ceiling as a fraction of `timeLeft` |
| `HARD_MAX_FRACTION`   | 0.35  | Hard ceiling as a fraction of `timeLeft` |
| `HARD_MULT`           | 2.5   | Hard ceiling as a multiple of the base target |
| `START_NEXT_FRACTION` | 0.60  | Do not open a new depth past this share of `soft` |
| `OPENING_MOVES`       | 6     | Length of the opening budget ramp |
| `OPENING_MIN_SCALE`   | 0.50  | Base fraction on the very first move |
| `STABILITY_MIN/MAX`   | 0.5 / 2.0 | Bounds of the stability multiplier |

## Bypass modes

`useTimeManagement()` is `false` (no budget, no watchdog) when the `go`
command is `infinite`, `ponder`, or carries no clock and no `movetime`.

- `infinite` deepens up to `MAX_PLY` and relies on an external `stop`.
- `go depth N` / `go nodes N` keep their own cap.
- **`ponder`** searches a *bounded* fixed depth (`DEFAULTDEPTH`), **not**
  `MAX_PLY`. This engine's `ponderhit` only clears the ponder flag — it does
  not convert a running search into a timed one — so an unbounded ponder
  search would never return a move on `ponderhit` and the engine would flag.
  A bounded ponder search completes during the opponent's time; `ponderhit`
  (or `stop`) then emits the already-computed move.
- `go` with no arguments falls back to the default fixed depth.

## Integration points

- `engine/time/time_manager.hpp` / `.cpp` — `engine::time::Limits` (parsed
  UCI limits) and `engine::time::TimeManager` (budget + watchdog).
- `uci/uci.cpp` — `go` parses `wtime btime winc binc movestogo movetime
  nodes depth infinite ponder` into a `Limits`.
- `engine/engine.cpp` — `Engine::searchUCI(const time::Limits&)` selects the
  side's clock, derives `movesPlayed` from the full-move clock, initialises
  and starts the `TimeManager`, runs the search, then stops it.
- `engine/search/searcher.{hpp,cpp}` — `SearchRuntime::timeManager` (a
  nullable pointer; null for fixed-depth / perft / ponder paths, which are
  therefore unchanged). `runIterativeDeepening` consults
  `shouldStartNextDepth()` between depths and feeds `updateStability()`
  after each completed depth.
- `makefile` — `engine/time/` added to the engine source/header globs.

## Behaviour check

`go movetime {300,800,2000}` from the start position returned a move with a
measured `go`→`bestmove` latency of ~260 / 669 / 2030 ms — always within the
requested budget; the small overshoot is exactly what `MOVE_OVERHEAD_MS`
absorbs so the real GUI clock never flags.

A simulated 3+2 game (`wtime 180000 ... winc 2000`, consecutive `go`
commands in one process) plays each move in ~1–7 s, bounded by the per-move
hard ceiling, with no hang across moves. Move-1 budget stays at ~1 % of the
clock from 30 s up to 30 min controls. The performance suite is unchanged
(fixed-depth search node counts identical, since the time manager is
inactive on that path).

> Ponder freeze (fixed): the first cut routed `go ponder` to a `MAX_PLY`
> unbounded search with no budget. Because `ponderhit` does not stop or
> convert a running search here, the engine never returned a move once the
> predicted reply was played (e.g. Ruy López Exchange after `e4 e5 Nf3 Nc6
> Bb5 a6 Bxc6 dxc6`) and flagged. Ponder is now bounded to `DEFAULTDEPTH`,
> restoring the pre-time-management behaviour.
>
> Note on an earlier regression: with the initial constants
> (`HARD_MULT=5`, `OPT_MAX_FRACTION=0.45`, `HARD_MAX_FRACTION=0.85`,
> `STABILITY_MAX=2.5`) a single move could consume a large multiple of an
> already-large `timeLeft/mtg` base, so in non-bullet games the engine sank
> minutes into one opening move and ran into terminal time trouble. The
> tightened values above cap any single move to a small fraction of the
> remaining clock.
