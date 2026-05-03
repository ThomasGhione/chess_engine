# Design: Fully Automatic Evaluator Auto-Tuner

## 1. Goal

Build a fully automatic tuner for evaluator constants.

The tuner must:

1. read the list of tunable weights from `finetuner/auto_tuner.csv`;
2. create a modified engine `H_new` from the current best engine `H_old`;
3. compile both engines automatically;
4. run self-play matches from a fixed FEN suite;
5. decide statistically if the change is good;
6. keep the new value or rollback automatically;
7. continue with the next weight until the stop condition is reached.

The first version should tune one weight at a time. After this works, the same
infrastructure can be extended to SPSA or Texel tuning.

## 2. Legend

- `H_old`: current accepted engine.
- `H_new`: engine with one candidate weight change.
- `w`: one evaluator weight.
- `w0`: current accepted value of `w`.
- `w+`: candidate value `w0 + step`.
- `w-`: candidate value `w0 - step`.
- `N`: number of FEN positions in the tuning suite.
- `G`: number of games in one test batch.
- `I`: one iteration. In this design, `I = 2N` games because every FEN is played
  twice, once with `H_new` as White and once with `H_new` as Black.
- `r`: game result from `H_new` point of view.
- `S`: total score of `H_new` in one iteration.
- `R`: final iteration result, stored as `(S, p, Elo, SE, LCB, UCB)`.
- `p`: average score of `H_new`, also called score rate.
- `Elo`: estimated Elo difference between `H_new` and `H_old`.
- `SE`: standard error of `p`.
- `LCB`: lower confidence bound of `p`.
- `UCB`: upper confidence bound of `p`.
- `cp`: centipawns.

## 3. Assumptions

- All tunable evaluator constants are centralized in `engine/eval_constants.hpp`.
- Every tunable weight appears in `finetuner/auto_tuner.csv`.
- Both engines expose the UCI protocol.
- Pondering is disabled during tuning.
- The same time control, depth, thread count and hash size are used for both
  engines.
- The FEN suite is fixed during a tuning run.
- Each FEN is played with color swap to reduce first-move and position bias.
- The tuner modifies only evaluator constants, not search logic.
- A candidate is accepted only if it passes both statistical validation and
  basic sanity tests.
- The first implementation uses coordinate descent: one parameter is changed at
  a time.

## 4. Technologies

- C++23: chess engine and evaluator constants.
- Python 3: orchestration, file patching, UCI process control, statistics.
- CSV: list of tunable weights and tuning bounds.
- FEN/EPD: starting positions for self-play.
- UCI: communication with `H_old` and `H_new`.
- Makefile: automatic compilation.
- PGN: optional game logs for debugging.
- JSON/CSV reports: machine-readable tuning history.
- Git: optional checkpointing after accepted batches.

## 5. Input Files

### 5.1 `finetuner/auto_tuner.csv`

Recommended schema:

```csv
w_name,w_default,w_min,w_max,step,enabled,group
QUEEN_VALUE,975,875,1075,10,1,material
PASSED_PAWN_BONUS,28,10,70,2,1,pawns
```

Fields:

- `w_name`: exact constant name in `eval_constants.hpp`.
- `w_default`: initial value.
- `w_min`: minimum allowed value.
- `w_max`: maximum allowed value.
- `step`: initial tuning step.
- `enabled`: `1` means tune this weight, `0` means skip it.
- `group`: optional label, for example `material`, `king`, `pawns`, `mobility`.

If `w_min`, `w_max` or `step` are missing, compute them automatically.

### 5.2 FEN Suite

Recommended file:

```txt
finetuner/positions.fen
```

The suite should contain:

- openings after development started;
- quiet middlegames;
- tactical middlegames;
- rook endgames;
- pawn endgames;
- winning conversion positions;
- defensive hold positions.

Avoid illegal FENs, forced mates, repeated duplicates and positions where one
side is already trivially winning.

## 6. Weight Bounds and Step Formulas

For each weight `w0`, if bounds are not specified:

```txt
range = max(1, round(abs(w0) * 0.10))
w_min = w0 - range
w_max = w0 + range
```

Meaning:

- the default search range is `+-10%` around the current value;
- `abs(w0)` is used so negative penalties work correctly;
- `max(1, ...)` prevents zero-width ranges for small constants.

If `step` is not specified:

```txt
step = max(1, round(abs(w0) * 0.01))
```

Meaning:

- the default step is `1%` of the current absolute value;
- small constants still move by at least `1`.

Candidate values are clamped:

```txt
w_candidate = min(w_max, max(w_min, w0 + direction * step))
```

where:

- `direction = +1` tests an increase;
- `direction = -1` tests a decrease.

## 7. Self-Play Scoring

Each FEN produces two games:

1. `H_new` White vs `H_old` Black.
2. `H_old` White vs `H_new` Black.

Score every game from `H_new` point of view:

```txt
win  = 1.0
draw = 0.5
loss = 0.0
```

Total score:

```txt
S = sum(r_i)
```

Average score:

```txt
p = S / G
```

where:

- `r_i` is the result of game `i`;
- `G` is the number of played games.

Interpretation:

- `p = 0.50`: equal strength.
- `p > 0.50`: `H_new` performed better.
- `p < 0.50`: `H_new` performed worse.

## 8. Elo Estimate Formula

Convert score rate to Elo:

```txt
p_safe = min(0.999, max(0.001, p))
Elo = 400 * log10(p_safe / (1 - p_safe))
```

Explanation:

- Elo expectation is `p = 1 / (1 + 10^(-Elo / 400))`.
- Solving that equation for `Elo` gives the formula above.
- `p_safe` avoids division by zero when `p` is exactly `0` or `1`.

The Elo value is only an estimate. The accept/reject decision should use
confidence bounds, not raw Elo alone.

## 9. Statistical Decision Formula

Use sample standard error:

```txt
mean = p
variance = sum((r_i - mean)^2) / (G - 1)
SE = sqrt(variance / G)
```

Explanation:

- `variance` measures how noisy the game results are.
- `SE` estimates how much the measured score rate may move if the test is
  repeated.

Use a one-sided lower confidence bound:

```txt
LCB = p - z * SE
```

Recommended value:

```txt
z = 1.64
```

This is approximately a 95% one-sided confidence bound.

Accept a candidate if:

```txt
LCB > 0.50 + accept_margin
```

Recommended:

```txt
accept_margin = 0.005
```

Meaning:

- the tuner does not accept a weight just because it scored slightly above 50%;
- it accepts only if the lower bound is still above equality plus margin.

Reject early if:

```txt
UCB = p + z * SE
UCB < 0.50 - reject_margin
```

Recommended:

```txt
reject_margin = 0.005
```

Meaning:

- if even the upper bound is below equality, the candidate is very likely bad;
- stop the batch early and rollback.

## 10. Automatic Tuning Algorithm

For every enabled weight:

1. Read `w0` from `eval_constants.hpp`.
2. Read `w_min`, `w_max`, `step` from CSV or compute them.
3. Build `H_old`.
4. Try `w+ = clamp(w0 + step)`.
5. Patch `eval_constants.hpp` for `H_new`.
6. Build `H_new`.
7. Run one self-play iteration `I = 2N`.
8. Compute `S`, `p`, `SE`, `LCB`, `UCB`, `Elo`.
9. If accepted, keep `w+` and mark it as new `w0`.
10. If rejected, rollback to `w0`.
11. Try `w- = clamp(w0 - step)`.
12. Repeat the same test for `w-`.
13. If `w-` is accepted, keep it.
14. If both directions fail, keep `w0`.
15. Move to the next weight.

After all weights are processed, one tuning pass is complete.

## 11. Step Adaptation

After each weight test, update its step automatically.

If a candidate is accepted:

```txt
step_next = min(step_max, round(step * 1.25))
```

Meaning:

- if the direction is good, move a little faster next time.

If both `w+` and `w-` fail:

```txt
step_next = max(1, floor(step * 0.50))
```

Meaning:

- if neither direction works, the current step may be too large.

Stop tuning a weight when:

```txt
step_next == 1
```

and both directions fail with `step = 1`.

## 12. Full Automation Pipeline

The Python runner should execute this sequence:

1. Load `auto_tuner.csv`.
2. Validate every weight against `eval_constants.hpp`.
3. Load and validate the FEN suite.
4. Copy the current source tree to a temporary `old` build directory.
5. Copy the source tree again to a temporary `new` build directory.
6. Compile `H_old`.
7. For each candidate:
   - patch only `new/engine/eval_constants.hpp`;
   - compile `H_new`;
   - launch both binaries in UCI mode;
   - run paired games from all FENs;
   - save PGN, CSV result row and engine logs;
   - accept or reject automatically;
   - update `auto_tuner.csv` with the new accepted value and step;
   - update the real `eval_constants.hpp` only after acceptance.
8. At the end, run sanity tests.
9. Write a final report.

Recommended output:

```txt
finetuner/runs/<timestamp>/
  games.pgn
  results.csv
  accepted_weights.csv
  rejected_weights.csv
  final_report.md
```

## 13. Sanity Tests Before Accepting a Full Pass

After a full tuning pass, run:

1. `make prod`
2. `make test`
3. fixed tactical-position test suite
4. fixed endgame-conversion test suite
5. short match `H_tuned` vs `H_previous`

Reject the full pass if:

- compilation fails;
- legal move generation breaks;
- tactical suite loses known winning moves;
- endgame suite starts accepting bad draws;
- short match score is clearly negative.

## 14. Stop Conditions

Stop the whole tuner when one condition is true:

- maximum runtime reached;
- maximum number of passes reached;
- no weight accepted in a full pass;
- every active weight reached `step = 1` and both directions failed;
- validation match fails;
- user-created stop file exists:

```txt
finetuner/STOP
```

The stop file allows safe manual interruption without corrupting the current
accepted weights.

## 15. Important Practical Rules

- Always play color-swapped pairs.
- Disable pondering.
- Use the same hash and thread settings for both engines.
- Use deterministic random seeds when shuffling FENs.
- Do not tune too many correlated weights in the first implementation.
- Tune material values carefully and with smaller steps than positional terms.
- Keep every accepted value inside CSV bounds.
- Save every result; never overwrite old run directories.
- Never accept a candidate based only on one lucky batch.

## 16. Future Extensions

### 16.1 SPSA

SPSA changes many weights at once by testing two vectors:

```txt
W_plus  = W + c * Delta
W_minus = W - c * Delta
```

where:

- `W` is the vector of current weights;
- `c` is the perturbation size;
- `Delta` is a random vector where each value is `+1` or `-1`.

The gradient estimate for weight `i` is:

```txt
g_i = (score(W_plus) - score(W_minus)) / (2 * c * Delta_i)
```

Then update:

```txt
W_i_next = W_i + a * g_i
```

where `a` is the learning rate.

SPSA is faster for many weights, but it is harder to debug. Implement coordinate
descent first.

### 16.2 Texel Tuning

Texel tuning optimizes evaluator weights against labeled positions.

For a position with engine eval `e` in centipawns, convert eval to expected score:

```txt
P = 1 / (1 + exp(-K * e))
```

where:

- `P` is expected result from White point of view;
- `e` is static evaluation in centipawns;
- `K` controls how quickly eval maps to win probability.

Minimize mean squared error:

```txt
MSE = sum((P_i - R_i)^2) / M
```

where:

- `R_i` is the real game result for position `i`;
- `M` is the number of labeled positions.

Texel is useful for fast evaluator fitting. Self-play is still required to
confirm Elo gain.

## 17. First Implementation Checklist

1. Create complete `auto_tuner.csv`.
2. Create `positions.fen`.
3. Implement Python CSV parser.
4. Implement safe patcher for `eval_constants.hpp`.
5. Implement old/new build directories.
6. Implement UCI match runner.
7. Implement color-swapped FEN loop.
8. Implement score, Elo and confidence formulas.
9. Implement accept/reject/rollback.
10. Implement run reports.
11. Run a small test with one weight.
12. Scale to all enabled weights.
