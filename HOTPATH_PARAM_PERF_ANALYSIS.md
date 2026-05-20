# Hot-path parameter & performance analysis

Profile-driven analysis of the hot code (searcher, sorter, evaluator, move
generator, board, tt). Goal: less complexity / fewer LOC, **and above all
more performance**. Real performance wins are kept strictly separate from
perf-neutral ergonomic/LOC refactors.

> **Status:** all behaviour-identical perf wins from this analysis have
> been implemented and pushed. The remaining items are either explicitly
> perf-neutral (cosmetic cleanup), speculative-marginal, or
> behaviour-changing (require cutechess SPRT, not node-count A/B). See §5.

> **Premise correction.** Reducing parameters or turning static utility
> classes into stateful objects does **not** generically make code faster.
> `const Board&` is already a pointer in a register; precomputed
> bitboard/scalar arguments are register-resident. Replacing them with
> member loads is usually perf-neutral or a small regression. Real
> speedups came from **not recomputing the same thing twice per node**
> and from sizing software caches correctly.

---

## 1. Methodology

* **Profilers:** `perf` blocked on this host (`perf_event_paranoid=4`);
  **`valgrind/callgrind`** is the primary deterministic profiler (counts
  instructions, not wall-clock — no noise). gprof (`make debug`) is
  secondary for self-time % cross-checks. `--separate-callers=2` on
  callgrind is essential to split "function is hot" from "the *recompute*
  of this function is hot".
* **Workload:** `position startpos` + `go depth 11`, `OMP_NUM_THREADS=1`
  for deterministic single-thread profiles. For runs that interact with a
  busy machine, pipe in `sleep ≥ 5min` before `quit` so the engine isn't
  aborted mid-search by `stop`/`quit` reaching the UCI handler first.
* **Correctness anchor:** node count A/B equality via `git stash` of the
  changed files. The absolute value isn't the invariant — *equality
  across the A/B* is (so user-side eval-constant tuning doesn't
  contaminate the comparison).
* **Validation protocol for any change:** node count A/B equal + UCI
  bestmove A/B equal + callgrind `--separate-callers=2` Ir delta on the
  targeted bucket. Wall-clock is too noisy to be trustworthy alone.

---

## 2. What was done (closed)

| Item | Outcome |
|---|---|
| **`sortLegalMoves(SearchRuntime&)` bundling** (α) | 13 → 10 params (4 heuristic-array refs → one `SearchRuntime&`). Required extracting `SearchRuntime` from `Searcher` to namespace scope so `sorter.hpp` can forward-declare it (avoids include cycle). Behaviour bit-identical via stash A/B. |
| **`MoveOrderingContext` bundling** | 4 heuristic-array refs in the struct → one `const SearchRuntime&`. ~24 B smaller per ctx instance; ergonomic-only (perf-neutral). |
| **`P0.2` quiet-gives-check carry-forward** | `MovePickerData` carries a per-move tri-state memo computed by ordering; `searchMoves` reuses it instead of recomputing `givesCheckAfterQuietMoveFast`. Measured: gprof self-time for that function 7.38% → 6.27% (−1.11 pp). Bit-identical via A/B. |
| **`P0.1` `lmrLosingCapture` dead branch removed** | The SEE call was on the *post-move* board (after `doMove`), so the value was garbage: empirically `lmrLosingCapture==true` 0 times / 7 490 evaluations across 5 positions. Removed; bit-identical (`X || false ≡ X`). |
| **SEE cache: 4K → 64K + lighter key** | Cache hit-rate at 4K was only ~16% (84% collision rate ⇒ thrashing). Resizing to 64K and replacing the 3-multiply hash key with shifts gave a measured **−3.33% total Ir** at equal node count (33.85 G → 32.73 G); bucket SEE←sortLegalMoves: 14.25% → 11.47%. Cost: +8 MB total (1 MB/thread × 8). Bit-identical. |

Aggregate effect of the recompute-elimination + cache work: **measurable
total Ir reduction (~3-4% across the whole program)** at equal nodes,
verified deterministically.

---

## 3. Hot-path ranking — post all changes (callgrind, single-thread, depth 11)

Total Ir: **32.73 G** (down from 33.85 G).

| Ir % | Caller chain | Notes |
|--:|---|---|
| 11.47 | `staticExchangeEvaluation` ← `sortLegalMoves` ← `searchPosition` | Legitimate ordering cost. Was 14.25% before cache work. Further wins here would change behaviour (skip-SEE heuristics) and need SPRT. |
| 6.34 | `evalKingSafetySide` ← `evaluateOpeningPhase` ← `evaluate` | Inherent eval cost. |
| ~6 | `givesCheckAfterQuietMoveFast` ← `sortLegalMoves` ← `searchPosition` | Legitimate ordering cost; recompute already eliminated by P0.2. |
| ~4 | `evalPawnsByColor` ← `evalPawnStructure` ← `evalPawnStructureCached` | Inherent eval cost; cache already in place. |
| 3.10 | `doMove` ← `searchPosition` ← `searchRootMoveScore` | Inherent make-move cost. |
| 3.08 | `evaluateOpeningPhase` ← `evaluate` ← `quiescenceSearch` | qsearch stand-pat. |
| 2.30 | `computeAttackData` ← `evaluate` ← `quiescenceSearch` | qsearch attack-data build. |
| 2.00 | `evaluateOpeningPhase` ← `evaluate` ← `searchPosition` | interior-node staticEval. |
| 1.79 | `quiescenceSearch` ← `searchPosition` | inherent. |
| 1.54 | `undoMove` ← `searchPosition` ← `searchRootMoveScore` | inherent. |
| 1.46 | `computeAttackData` ← `evaluate` ← `searchPosition` | interior-node attack-data. |
| 0.57 | `staticExchangeEvaluation` ← `quiescenceSearch` ← `searchPosition` | qsearch SEE (separate use). |

After this work, **no remaining bucket is dominated by recomputation**.
The big buckets are all legitimate work.

---

## 4. Tooling lessons (worth keeping)

* **gprof flat self-time over-attributes to "function X is hot"** without
  telling you whether the cost is the originating computation or a
  recompute. The earlier write-up of this doc claimed P0.1 (lmrLosingCapture
  SEE) would save the 14.8% SEE bucket; that was wrong by ~3 orders of
  magnitude — the recompute was actually 0.004% of Ir. Always corroborate
  with callgrind `--separate-callers=2` before promising a perf win.
* **Bit-identical claims must be tested via `git stash` A/B**, not against
  a previous absolute node count, because user-side eval-constant tuning
  shifts the baseline.
* **Under valgrind**, pipe stdin with `( ... ; sleep 420 ; printf 'quit\n' )`
  to avoid the UCI handler reading `quit` mid-search.
* **Hardware cache locality matters at large sizes.** Software caches
  thread-local at 4 MB/thread (= 256K SEE entries) overflow L2; 1 MB/thread
  (= 64K) stays comfortable. callgrind measures instructions, not cache
  misses, so above ~L2/thread the Ir delta becomes an upper bound on the
  wall-clock delta.

---

## 5. What's still on the table

### 5.1 Documentation / closure

* Nothing left here once this `.md` is committed.

### 5.2 Speculative-marginal (low confidence, possibly not worth it)

* **`P0.3` hoist initial-attacker masks** in `getLeastValuableAttackerTo`.
  Inside the SEE recompute path (now ~74% of SEE calls at 64K). Estimated
  saving: <0.5% Ir. Speculative; would need an A/B to be worth defending.

### 5.3 Behaviour-changing — need cutechess SPRT, not node-count A/B

These all change the search tree (so node count will move) and need a
strength test in the cutechess pipeline, not the cleanup-style A/B we used
in this doc.

* **Re-enable `lmrLosingCapture` correctly.** The original intent was to
  reduce LMR for losing captures via SEE < 0; the existing call used
  post-move SEE (garbage) and was empirically dead. Fixing it = computing
  SEE pre-move and carrying it via the ordering memo. This activates a
  tuned-but-never-fired feature; net Elo unknown.
* **Skip SEE in ordering for clearly-winning captures** (victim ≥ attacker
  value with no defender). Cuts SEE call rate sharply (currently ~30 M
  calls per depth-11 search); changes ordering decisions on borderline
  cases.
* **Smarter / different move ordering** (e.g., MVV/LVA-only for captures
  above a threshold; staged generation).
* **Tuning `eval_constants.hpp`** is owned by the user's cutechess
  pipeline; likely a bigger Elo lever than further code-level work.

### 5.4 Deep work (days, not hours)

* **Re-profile the eval pipeline** after this round. Eval cumulative is
  the largest remaining aggregate (~14%+ of Ir across king-safety / pawn /
  attack-data / opening-phase / hanging / outposts). It is already
  structured well (`computeAttackData` once per `evaluate`, `EvalCache`
  memoising terms across `do/undoMove`). A new profiling pass would
  hunt for sub-evaluators recomputing what `AttackData`/`EvalCache`
  could provide — speculative until measured.

---

## 6. Recommendation

The behaviour-identical perf-cleanup well is **closed**. Further
significant gains require either (a) a separate algorithmic/heuristic
investigation validated by cutechess SPRT, or (b) a deep re-profile of
the eval pipeline. Both are outside the "node-count A/B" methodology
that drove this round.

Tuning of `eval_constants.hpp` (user-side, cutechess) is the most
productive next lever for Elo.
