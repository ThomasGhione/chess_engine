# Hot-path parameter & performance analysis

Profile-driven analysis of the hot code (searcher, sorter, evaluator, move
generator, board, tt). Goal: less complexity / fewer LOC, **and above all
more performance**. Real performance wins are kept strictly separate from
perf-neutral ergonomic/LOC refactors.

> **Premise correction.** Reducing parameters or turning static utility
> classes into stateful objects does **not** generically make code faster.
> `const Board&` is already a pointer in a register; precomputed
> bitboard/scalar arguments are register-resident. Replacing them with
> member loads is usually perf-neutral or a small regression. Real
> speedups come from **not recomputing the same thing twice per node**.

---

## 1. Methodology & measurement (current)

* **Profiling tools:** `perf` is blocked on this host
  (`perf_event_paranoid=4`); **`valgrind`/`callgrind` is now installed**
  and is the primary deterministic profiler. gprof (`make debug`) is the
  secondary, for self-time % cross-checks. gprof's call-graph counts are
  unusable here (LTO + `-pg` breaks attribution); **callgrind
  `--separate-callers=2` is what splits work by caller**, which is
  essential to distinguish "function is hot" from "the *recompute* of
  this function is hot".
* **Workload:** `position startpos` + `go depth 11`, `OMP_NUM_THREADS=1`
  for deterministic single-thread profiles.
* **Correctness anchor:** **node count A/B equality** via `git stash` of
  the changed files — any "behaviour-identical" claim must show the
  *same* node count with and without the change, on the *same* working
  tree (so user-side eval-constant tuning doesn't contaminate the
  comparison). The current node count happens to be **7 524 212**
  (depending on `eval_constants.hpp` state), bestmove `e2e4 / g8f6`; but
  the absolute number isn't the invariant — equality across the A/B is.
* **Wall-clock perf:** noisy band; use ≥3 `./tests/perf` runs and
  compare **minimums** vs the A/B baseline (also from `git stash`),
  never against a stale absolute number.

---

## 2. What's already done (for context)

| Done | What | Outcome |
|---|---|---|
| ✅ | **P0.2** Carry quiet "gives-check" bit from ordering into `searchMoves` | Committed. Behaviour bit-identical (A/B at equal nodes). Measured: gprof `givesCheckAfterQuietMoveFast` 7.38% → 6.27% (-1.11 pp), wall-clock ~3–6% on the perf harness. The single concrete perf win from this analysis. |
| ✅ | **P0.1** (re-scoped) Remove `lmrLosingCapture` branch | **Not a perf win** (see §3). It was a bug: SEE was called *after* `doMove`, so it computed `PIECE_VALUES[mover_piece]` on a corrupted occupancy and was empirically false 0/7490 times across 5 positions. Removed as dead code, behaviour bit-identical. Saves ~0.004% Ir. Awaiting commit. |

The earlier ranking that claimed P0.1 would save the 14.8% SEE bucket was
**wrong** — see §3 for the corrected callgrind breakdown.

---

## 3. Hot-path ranking — corrected with callgrind `--separate-callers`

Total instructions per depth-11 single-thread run: **33.9 G Ir**. Top
buckets attributed by caller (not just function self-time):

| Ir % | Caller chain | Note |
|--:|---|---|
| **14.23** | `staticExchangeEvaluation` ← **`sortLegalMoves`** ← `searchPosition` | **Legitimate** ordering cost. Cannot be removed by carry-forward (it *is* the originating computation). To reduce: skip more moves from SEE in ordering, lighter cache key, or different ordering scheme — all behaviour-changing. |
| 6.34 | `evalKingSafetySide` ← `evaluateOpeningPhase` ← `evaluate` | Inherent eval cost. |
| 6.19 | `givesCheckAfterQuietMoveFast` ← `sortLegalMoves` ← `searchPosition` | Legitimate ordering cost (the *recompute* in `searchMoves` was already eliminated by P0.2). |
| 4.00 | `evalPawnsByColor` ← `evalPawnStructure` ← `evalPawnStructureCached` | Inherent eval cost; `Cached` suggests EvalCache memo already applied. |
| 3.10 | `doMove` ← `searchPosition` ← `searchRootMoveScore` | Inherent make-move cost. |
| 3.08 | `evaluateOpeningPhase` ← `evaluate` ← `quiescenceSearch` | qsearch stand-pat. |
| 2.30 | `computeAttackData` ← `evaluate` ← `quiescenceSearch` | qsearch stand-pat's attack-data build. |
| 2.00 | `evaluateOpeningPhase` ← `evaluate` ← `searchPosition` | interior node staticEval. |
| 1.92 | `evalPawnForks` ← `evaluateOpeningPhase` ← `evaluate` | inherent. |
| 1.79 | `quiescenceSearch` ← `searchPosition` | inherent. |
| 1.54 | `undoMove` ← `searchPosition` ← `searchRootMoveScore` | inherent. |
| 1.46 | `computeAttackData` ← `evaluate` ← `searchPosition` | interior node attack-data. |
| 0.71 | `staticExchangeEvaluation` ← `quiescenceSearch` ← `searchPosition` | qsearch SEE (separate use). |
| **0.004** | `staticExchangeEvaluation` ← `searchPosition` ← `searchRootMoveScore` | (this was the `lmrLosingCapture` recompute — removed.) |

**The big buckets that remain are all "legitimate work", not
recomputation.** Further behaviour-identical perf wins of the P0.2 kind
(recompute elimination) are not visible in the current callgrind: the
two obvious ones have been done.

---

## 4. What's still on the table

Honestly: little remains in the "behaviour-identical, measurable perf
win" category. The remaining ideas are speculative, perf-neutral, or
behaviour-changing.

### 4.1 P0.3 — SEE internals (speculative, **measure first**)

Inside the 14.23% ordering SEE bucket:

* **Lighter cache key.** Three 64-bit multiplies + xors per `staticExchangeEvaluation` call to compute `cacheKey`. A cheaper key (e.g.
  `hash ^ (from<<6) ^ (to<<12) ^ (promoCode<<18)`) would cut a few %
  off SEE's self-time *if* dispersion stays adequate (cache is 4096
  entries). Currently **unmeasured**: needs a hit-rate counter A/B
  before any decision. Behaviour-identical (cache value, not policy).
* **Cache sizing.** 4096 entries thread-local; collision rate at depth
  11 unknown. Resizing changes hit rate → could affect call count of
  SEE internals (but not SEE result), so behaviour-identical but
  measurable only with an A/B instrumented build.
* **Hoist initial-attacker masks.** The first `getLeastValuableAttackerTo`
  call computes bishop/rook rays already used inside the swap loop's
  first iteration — minor.

**Status:** worth attempting only with a temporary hit-rate counter to
measure cache effectiveness first. Without that, it's pure speculation.

### 4.2 P1 — parameter / LOC reduction (perf-neutral, labelled honestly)

These reduce complexity and LOC. They are **not** performance wins —
several pass register-resident precomputed values, so bundling them
into context structs is at best perf-neutral.

| Function | Params | Note |
|---|--:|---|
| `Sorter::sortLegalMoves` | 13 | history/killer/counter/captureHistory `(&)[…]` are pointer-passes (cheap). Could bundle by passing `SearchRuntime&` (it owns them) — **LOC/cohesion win, perf-neutral**, consistent with prior intrinsic-move refactors. Medium blast radius. **Recommended if you want clean closure on the P1 line.** |
| `Evaluator::evalKingSafetySide` | 9 | bools/scalars are register args; bundling = clarity-only. Optional. |
| `Evaluator::evalPawnsByColor` | 9 | same. Optional. |

### 4.3 Out of scope here (would require strength testing)

* **Restore `lmrLosingCapture` as a real feature** (compute SEE
  pre-move via P0.2-style carry from ordering). This is the "fix the
  intent" path. Node count *will* change (the feature would now actually
  fire); strength impact unknown → cutechess SPRT required, not
  node-count A/B.
* **SEE cache resize / heuristic simplification / different ordering.**

---

## 5. Recommendation: how to proceed

1. **Commit current state** — P0.1-B dead-code removal sits uncommitted
   in `engine/search/searcher.cpp`. Bit-identical A/B verified; ready.
2. Then **pick one**:
   * **(α) Close the P1 line** — implement `sortLegalMoves(SearchRuntime&)`
     bundling. Clean LOC win, perf-neutral, matches prior refactor style.
     Honest: not a perf gain.
   * **(β) Re-profile post-P0.2 + P0.1-B for a v2 perf hunt** — the
     current callgrind was on P0.2-applied only; a fresh callgrind on
     the cleaned-up state may surface different opportunities (probably
     in the eval pipeline — `EvalCache` reuse, `computeAttackData`
     redundancy across qsearch+search). Will probably show that the
     easy wins are done; expect speculative P0.3-style ideas at best.
   * **(γ) Park perf, move on** — your `eval_constants.hpp` tuning is
     likely a bigger Elo lever than further code-level optimization.

My honest take: **(α) for tidy closure**, then (γ). The behaviour-
identical perf-win well is essentially dry; further significant Elo
comes from tuning (your area) or from algorithm changes (separate task
with cutechess validation).
