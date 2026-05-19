# Hot-path parameter & performance analysis

Profile-driven analysis of the hot code (searcher, sorter, evaluator, move
generator, board, tt). Goal: less complexity / fewer LOC, **and above all
more performance**. Per the agreed methodology, real performance wins are
kept strictly separate from perf-neutral ergonomic/LOC refactors.

> **Premise correction (agreed upfront).** Reducing parameters or turning
> static utility classes into stateful objects does **not** generically make
> code faster. `const Board&` is already a pointer passed in a register;
> precomputed bitboard/scalar arguments are register-resident — the fastest
> calling convention. Replacing them with member loads is usually
> perf-neutral or a small regression. The static→stateful idea was therefore
> **dropped**. Real speedups here come from **not recomputing the same thing
> twice per node**, which is what P0 targets.

---

## 1. Methodology & measurement

* **Profiler:** `perf` is blocked on this host (`perf_event_paranoid=4`,
  non-interactive `sudo` needs a password/TTY); `valgrind`/`callgrind` is not
  installed. Fallback is the project's own path: `make debug` (`-pg`) + gprof.
* **Workload:** `position startpos` + `go depth 11`, run with
  `OMP_NUM_THREADS=1` so the profile is **single-thread and deterministic**.
  Node count is the correctness anchor: **2 171 395** (unchanged across the
  whole cleanup series; identical at 1 and 8 threads).
* **Caveat:** the debug build is `-O1` (+ LTO), so functions inlined at `-O3`
  show separately and self-time is approximate. gprof's call-graph counts are
  unusable here (LTO + `-pg` breaks attribution); **flat self-time %** is the
  reliable signal and is what the ranking below uses. Call frequency is
  derived by reading call sites against the node count, not from gprof.
* **Validation protocol for any change from this doc:**
  1. `make perf` → node count **must stay 2 171 395** (bit-identical search;
     these are pure dedup/recompute-elimination, not heuristic changes).
  2. UCI `go depth 11` from startpos → bestmove **must stay `e2e4 / g8f6`**.
  3. Re-profile with the same `make debug` + gprof single-thread run; compare
     **self-time %** of the targeted function before/after (deterministic
     enough for a clear delta on a 15%/7% bucket).
  4. nps over ≥10 prod runs as a secondary signal (noisy band 152–217 ms;
     only large moves are trustworthy here).

---

## 2. Hot-path ranking (gprof flat, single-thread, depth 11)

| % self | Function | Category |
|--:|---|---|
| 16.3 | `Searcher::searchPosition` | node driver (much inlined work attributed here) |
| **14.8** | `Sorter::staticExchangeEvaluation` (SEE) | **P0 — recomputed** |
| **6.6** | `Sorter::givesCheckAfterQuietMoveFast` | **P0 — recomputed, uncached** |
| 6.1 | `Sorter::sortLegalMoves` | move ordering / node |
| 5.6 | `Evaluator::evalKingSafetySide` | eval pipeline |
| 5.1 | `Searcher::quiescenceSearch` | node driver |
| 4.6 | `Evaluator::evalPawnsByColor` | eval pipeline |
| 4.6 | `Board::inCheck` | per-node/per-move check test |
| 4.1 | `Evaluator::computeAttackData` | eval pipeline (already 1×/eval) |
| 3.6 | `Evaluator::evaluateOpeningPhase` | eval pipeline |
| 3.1 | `MoveGenerator::generateLegalMovesFor<false>` | move generation |
| 3.1 | `Evaluator::evalPawnForks` | eval pipeline |
| 2.0 | `emitAllNonPawnLegal` / `evalPawnStructure` | movegen / eval |
| 1.5 | `Board::undoMove` / `writeTT` / `Evaluator::evaluate` | make/unmake, TT |

The eval sub-functions together (~26% cumulative) are the largest aggregate,
but they are already structured well (single `computeAttackData` per
`evaluate`, `AttackData*` threaded to sub-evaluators, `EvalCache` on `Board`
memoising terms across do/undo). The clearest **recomputation** waste is SEE
and the quiet-check predicate.

---

## 3. P0 — real performance (recomputation elimination)

These remove work that is provably done twice for the same move at the same
node. Highest priority; measurable on the 15%/7% buckets.

### P0.1 — Carry SEE from move ordering into the search loop  ★ highest value

**Evidence.** `staticExchangeEvaluation` is **14.8%** self-time (#2).

**Recomputation (verified by reading call sites):**
* `sorter.cpp:302` (`sortLegalMoves`): `see = needsSee ? staticExchangeEvaluation(b,m) : 0`, where
  `needsSee = !isHashMove && (isCapture || (!isPromotionCandidate && fromPieceType != KING))`.
  → SEE is computed during ordering for **every non-hash capture** (and most
  quiets).
* `searcher.cpp:650` (`searchMoves`, `lmrLosingCapture`):
  `Sorter::staticExchangeEvaluationPublic(b,m) < 0` for moves that are
  `wasCapture && !isFirstMove && !isPromotion && !inCheck && depth>=4`.
  Every such capture **already had SEE computed in ordering** (hash move is
  `moveIndex 0` = `isFirstMove`, which `lmrLosingCapture` excludes). The
  thread-local SEE cache turns this into a *hit*, but a hit still costs the
  3-multiply/xor key hash + mask + array load + key compare, per qualifying
  capture, per node.

**Fix.** `MovePickerData` already owns the parallel `scores[MAX_MOVES]`
array. Add `int16_t see[MAX_MOVES]` (SEE fits int16; clamp). `sortLegalMoves`
writes it where it already computes `see`. `nextMove()`'s selection-sort swap
gains one paired line (it already swaps `moves`/`scores`). `searchMoves`
reads `movePicker.seeAt(idx)` instead of calling SEE again — eliminating the
redundant call (and its cache traffic) for every late capture.

**Expected gain.** Removes the entire second SEE path for capture moves at
depth ≥ 4 (a large fraction of interior nodes). Even as cache hits, this is
inside the 14.8% bucket; eliminating it should move that bucket measurably.

**Risk / correctness.** None to search semantics: the value is identical
(same `b`, same `m`, same position — ordering runs before any `doMove`).
Verify node count stays 2 171 395. Low risk.

**LOC/complexity.** Roughly neutral (one array + 2–3 lines in picker, minus
the recompute site). The win is cycles, not lines — this is a **P0 perf**
item, not an LOC item.

### P0.2 — Carry the quiet "gives-check" bit forward  ★ high value, medium complexity

**Evidence.** `givesCheckAfterQuietMoveFast` is **6.6%** self-time (#3), and
unlike SEE it has **no cache** — every recompute is the full bitboard test.

**Recomputation:**
* `sorter.cpp:97` (`scoreMoveOrderingPriorityInline`): for a quiet,
  non-hash, non-capture, non-killer, non-counter, non-promo move it calls
  `givesCheckAfterQuietMoveFast(...)` to assign `CHECK_QUIET_SCORE`, then
  **throws the boolean away**.
* `searcher.cpp:615` (`searchMoves`, `preMoveGivesCheck`): recomputes
  `Sorter::givesCheckFast(b,m,...)` for quiet non-king moves to gate
  futility/LMP.

**Design caveat (must be respected).** Ordering short-circuits on
killer/counter **before** the check test (lines 80–90), so the bit is *not*
computed there for killer/counter quiets, while `preMoveGivesCheck` applies
to those too. Options:
* (a) Store the bit in `MovePickerData` only when ordering computed it; in
  `searchMoves` fall back to `givesCheckFast` only when "unknown". Net win
  without extra ordering work. **Recommended.**
* (b) Compute it unconditionally for all quiet non-king moves in ordering and
  store it — simpler, but adds work for killer/counter moves that often never
  reach the futility/LMP gate → could regress. **Not recommended without
  measurement.**

**Expected gain.** Removes an uncached 6.6%-bucket recompute for the common
case (non-killer quiets). Option (a) is strictly ≤ current work.

**Risk.** Behavioural-neutral if the stored bit is exactly
`givesCheckAfterQuietMoveFast` of the same pre-move position (it is). Verify
node count + bestmove. Medium *implementation* complexity (the
known/unknown plumbing), low correctness risk.

### P0.3 — SEE internals (micro, measure first)

`getLeastValuableAttackerTo` recomputes `getRookAttacks`/`getBishopAttacks`
for the target square as occupancy is peeled — inherent to the swap loop.
Cheaper, lower-confidence ideas (only if P0.1/P0.2 don't suffice, and only
with before/after self-time evidence):
* Reduce the SEE cache-key cost (three 64-bit multiplies + xors per call):
  the key is rebuilt even on the ordering pass. A lighter key (e.g.
  `hash ^ (from<<6 ^ to ^ promoCode<<12)`) keeps adequate dispersion at lower
  cost. **Speculative — must be measured; do not ship on reasoning alone.**
* The cache is 4096 entries thread-local; collision rate at depth 11 is
  unknown. Sizing/associativity changes are pure speculation without a
  hit-rate counter. **Out of scope until instrumented.**

---

## 4. P1 — parameter / LOC reduction (perf-neutral — labelled honestly)

These reduce complexity and line count. They are **not** performance wins;
several pass register-resident precomputed values, so collapsing them into
context structs is at best perf-neutral and can cost a memory load. Include
them for maintainability, not speed.

| Function | Params | Note |
|---|--:|---|
| `Evaluator::evalKingSafetySide` | 9 | `(b, wPawns, bPawns, AttackData*, 4×bool, side)` — bools/scalars are register args; bundling into a small `EvalSideCtx` is clarity-only, **perf-neutral**. |
| `Evaluator::evalPawnsByColor` | 9 | all scalar/bitboard, register-passed. Same: clarity-only. |
| `Evaluator::evaluateOpeningPhase` | 5 | `(b, phase, wPawns, bPawns, AttackData*)` — already lean; not worth touching. |
| `Sorter::scoreMoveOrderingPriorityInline` | 8 | already takes a `MoveOrderingContext&`; remaining args are per-move facts (cannot be hoisted without recomputation). Leave. |
| `Sorter::sortLegalMoves` | 13 | the history/killer/counter/captureHistory `(&)[…]` refs are pointer-passes (cheap). They could be bundled by passing `SearchRuntime&` (it owns them) — **LOC/clarity win, perf-neutral**, and consistent with the earlier intrinsic-move work. Medium blast radius (signature + call sites). |

**Recommendation:** do P1 only after P0, and only the `sortLegalMoves` →
`SearchRuntime&` bundling (real cohesion/LOC value, matches prior refactors).
Treat the eval-param bundling as optional cosmetic; explicitly **not** a
perf item.

---

## 5. P2 — complexity / LOC only (minor)

* Dead/constant-parameter elimination is largely done already (initialBest,
  cutoffValue, SearchRuntime methods, Board methods, C1–C3, fullSort move).
* Residual: the `MoveOrderingContext` and `SearchContext` structs duplicate a
  few fields derivable from `SearchRuntime`/`Board`; consolidating is
  LOC-only, perf-neutral, and risks churn. Low priority.

---

## 6. Explicitly rejected (with reasons)

* **static → stateful Evaluator/Sorter/Searcher** — dropped by decision:
  member loads replace register args; with YBWC/OpenMP a shared instance
  races (this is *why* `SearchRuntime` exists and `evalStack` is
  `thread_local`). Per-thread instances would add construction/indirection
  for no cycle benefit.
* **"Fewer parameters = faster"** — false for register-resident args; tracked
  as P1 perf-neutral, not perf.
* **Folding MoveGenerator/Evaluator into Board** — God-class; `const Board&`
  there is a genuine input, not a hidden receiver.
* **SEE cache resize / heuristic simplification** — would change node count
  (behaviour); out of scope for "behaviour-identical perf cleanup" and needs
  strength testing, not node-count verification.

---

## 7. Recommended order of execution

1. **P0.1** (SEE carry-forward) — highest value, lowest risk, behaviour-
   identical, clear measurement on the 14.8% bucket.
2. **P0.2 option (a)** (quiet gives-check carry-forward, known/unknown
   plumbing) — high value, no cache to hide the win, medium implementation
   care.
3. Re-profile; decide if **P0.3** is worth instrumenting.
4. **P1** `sortLegalMoves(SearchRuntime&)` bundling — LOC/cohesion, perf-
   neutral, do last.

Every step gated by: node count `2 171 395` + bestmove `e2e4/g8f6` + gprof
self-time delta on the targeted function.
