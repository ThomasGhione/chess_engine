# HydraY Chess Engine

HydraY is a C++23 chess engine. It has a terminal interface and full UCI support
for GUIs, bots, and automated testing.

It is built around bitboards with magic sliding-piece attacks, an iterative
deepening alpha-beta/PVS search, a cache-friendly transposition table, and a
handcrafted evaluator. Development and testing happen mainly on Linux/WSL; a
MinGW target exists for Windows builds.

## Requirements

- `g++` with C++23 support
- GNU `make`
- OpenMP (`-fopenmp`)
- Optional analysis tools: `valgrind`, `clang-tidy`, `scan-build`,
  `include-what-you-use`, `cppclean`, `lizard`, `perf`
- Optional for Windows builds: `mingw-w64`
- Optional for tuning/testing: `chess-tuning-tools`, `cutechess-cli`,
  `fastchess`, and `ordo`

## Quick Start

```sh
make prod        # build the optimized engine -> ./chess
./chess          # interactive terminal menu
./chess uci      # UCI mode
```

Example UCI session:

```txt
uci
isready
position startpos moves e2e4 e7e5
go depth 10
quit
```

The engine also switches to UCI mode automatically when stdin is piped, which is
what GUIs and bot runners do.

## Run Modes

```sh
./chess              # interactive menu
./chess uci          # UCI mode (-uci and --uci also work)
./chess -pvp         # human vs human
./chess -pvb w       # human (White) vs engine
./chess -pvb b       # human (Black) vs engine
./chess -bvb         # engine vs engine
```

In terminal game mode, enter moves as source and destination squares, e.g.
`e2 e4`. Promotions are entered when the game asks for them.

## Build Targets

```sh
make prod            # optimized production build -> ./chess
make debug           # debug build with symbols and profiling
make prod_windows    # cross-compile -> ./chess.exe (needs mingw-w64)
make cls             # remove binaries, object files, and temp files
make help            # show build-system help
```

Production builds use `-O3 -march=native`, OpenMP, and LTO. Object files go to
`output/` so incremental builds stay fast.

## Testing

```sh
make test            # functional tests -> ./tests/test
make perf            # performance tests -> ./tests/perf
make sacrifice       # anti-sacrifice regression suite -> ./tests/sacrifice
make all-tests       # build and run functional + performance tests
make test-valgrind   # run functional tests under valgrind
```

Use `make perf` to compare node counts at a fixed depth before and after any
change to a hot path. Functional tests alone do **not** catch search/eval
strength regressions.

### SPRT (is the change really stronger?)

`tuning/run_sprt.sh` plays the current build against a frozen baseline under a
time control and runs a sequential test (SPRT) that stops as soon as the change
is accepted (H1) or rejected (H0). Search changes must be tested under a time
control, never at fixed depth.

```sh
make prod && ./tuning/run_sprt.sh --snapshot   # freeze baseline BEFORE editing
# ...make your change...
make prod && ./tuning/run_sprt.sh              # new vs baseline
```

The backend is **fastchess** when it is on your PATH (pentanomial model, needs
fewer games for the same decision); it falls back to `cutechess-cli`. Force one
with `SPRT_BACKEND=fastchess|cutechess`.

Env knobs (defaults): `TC=4+0.04`, `ELO0=0 ELO1=5` (gain test; use
`ELO0=-3 ELO1=3` to prove a cleanup is not a regression), `CONCURRENCY`,
`HASH`, `THREADS`.

Watch a running test live (the SPRT verdict comes from fastchess itself; ordo
just shows Elo and error bars from the PGN):

```sh
watch -n 5 'ordo -q -s 1000 -J -p "$(ls -t tuning/sprt_*.pgn | head -1)"'
```

### Gauntlet (absolute Elo on a fixed scale)

`tuning/run_gauntlet.sh` answers "how strong are we overall?" instead of "better
than the last baseline?". The current build plays frozen release tags, then
[ordo](https://github.com/michiguel/Ordo) turns the PGN into ratings with one
old release pinned at a constant Elo, so runs stay comparable over time.

```sh
make prod && ./tuning/run_gauntlet.sh            # vs tag 1.2.0, 400 games
REF_TAGS="1.2.0" GAMES=1000 ./tuning/run_gauntlet.sh
```

Reference binaries are built on demand in a throwaway `git worktree` (your
checkout is never touched) and cached as `tuning/chess_ref_<tag>`. `ordo` must be
on your PATH (the script also checks `~/.local/bin`).

Env knobs (defaults): `REF_TAGS=1.2.0`, `ANCHOR_TAG` / `ANCHOR_ELO=2000` (the
fixed yardstick — an internal value, not a CCRL rating), `GAMES=400` per
opponent, `TC=4+0.04`, `CONCURRENCY`, `THREADS`.

> A new anchor tag must have working time management. Tag 1.1.0 and older ignore
> the clock and forfeit on time at any real TC — sanity-check first:
> `printf 'position startpos\ngo wtime 4000 winc 40\n' | ./tuning/chess_ref_<tag> -uci`
> should reply in about 100 ms, not seconds.

## Transposition Table and Huge Pages

The transposition table holds 4 entries per 64-byte bucket, 1M buckets, about
64 MiB total. On Linux it tries explicit huge pages first, then transparent huge
pages, then normal heap allocation.

Control this with `CHESS_TT_HUGEPAGE`:

```sh
CHESS_TT_HUGEPAGE=auto ./chess   # default
CHESS_TT_HUGEPAGE=on   ./chess   # force/try huge pages
CHESS_TT_HUGEPAGE=off  ./chess   # disable huge pages
```

Enabled values: `on`, `1`, `true`, `force`. Disabled values: `off`, `0`,
`false`. A huge-page benchmark is available with `make tt-huge-bench`
(`./tests/tt_hugepage_bench --depth 10 --repeats 3`).

## UCI Options

General options:

```txt
PonderDebug          check, default false
SearchApiMutexGuard  check, default true
```

`SearchApiMutexGuard` can also be set at startup with
`CHESS_ENGINE_SEARCH_MUTEX_GUARD=false ./chess uci`.

Most constants from `engine/eval_constants.hpp` are also exposed as `spin`
options. UCI names are CamelCase (e.g. `PassedPawnBonus`); tuning configs may use
the underscore form (e.g. `PASSED_PAWN_BONUS`).

## Evaluator Tuning

HydraY tunes evaluator constants through self-play with `chess-tuning-tools` and
`cutechess-cli`. The workflow lives in `tuning/`:

```txt
tuning/base_config.json    shared engine, depth, rounds, and book settings
tuning/groups/*.json       tracked parameter groups
tuning/run_tune_local.sh   local launcher
tuning/chess_uci.sh        starts the engine in UCI mode for the tuner
```

The `tune` CLI must be on your PATH (it lives in a dedicated Python environment,
not in `tuning/.venv`). Verify your tools:

```sh
tune --help
cutechess-cli --version
```

Run a tracked parameter group (build first with `make prod`):

```sh
cd tuning
./run_tune_local.sh pawn_structure
./run_tune_local.sh king_attack_units
```

See `tuning/groups/` for the full list of groups (pawn structure, mobility, king
safety, threats, material, and more). `run_tune_local.sh` merges the chosen group
with `base_config.json`, writes the active `tuning_config.json`, and tracks each
dataset under `tuning/.tuning_state/` so unchanged runs resume and changed
configs start fresh.

Keep each run focused: tune at most about 8 related parameters at once.

### Reading Results

Watch progress with `tail -f tuning/log.txt`. Key lines:

```txt
Testing {...}          parameter point being tested right now
Current optimum: {...} best point the Bayesian model has found
Estimated Elo: X +- Y  model estimate for that optimum
```

Apply `Current optimum`, not a single lucky `Got Elo`. If its 90% confidence
interval includes zero, the result is still uncertain — confirm with more games.
To apply: stop with `Ctrl+C`, copy the optimum into `engine/eval_constants.hpp`,
rebuild with `make prod`, then re-validate with SPRT or a gauntlet.

## How the Engine Works

The board uses compact square storage plus piece bitboards, kept in sync at all
times. Sliding attacks use magic bitboard tables built at startup. Zobrist
hashing tracks positions for the transposition table and repetition detection.

Search is iterative deepening over alpha-beta/PVS with: quiescence search,
transposition-table probing/storing, aspiration windows, null-move pruning,
reverse futility pruning, razoring, ProbCut, singular extensions, late move
reductions and pruning, killer/history/countermove/capture-history move
ordering, draw handling (stalemate, repetition, fifty-move, insufficient
material), and root parallelism.

The evaluator is handcrafted and split by piece and domain. It combines
material, piece-square tables, pawn structure, passed and candidate passed
pawns, king safety and activity, rook and queen activity, mobility, outposts,
threats, hanging and trapped pieces, coordination, bishop pair, castling, and
game-phase blending. All constants live in `engine/eval_constants.hpp`.

## Static Analysis

```sh
make analyze       # cppcheck, clang-tidy, iwyu, scan-build, GCC analyzer, cppclean, lizard
make complexity    # lizard complexity only
```

Reports are written to files such as `analysis.log`, `scan-build-report/`, and
`complexity-report.csv` (depending on which tools are installed).

## Project Layout

```txt
board/         board representation, FEN, move execution, legality, bitboards
engine/        engine runtime and tunable constants
engine/eval/   evaluator modules by piece and feature
engine/search/ move generation, move ordering, alpha-beta search
tt/            Zobrist hashing and transposition table
uci/           UCI protocol interface
driver/        terminal UI and game modes
tests/         functional, performance, and benchmark programs
tuning/        self-play tuning, SPRT, and gauntlet scripts
script/        benchmark and analysis helpers
```

## License

See `LICENSE`.
</content>
</invoke>
