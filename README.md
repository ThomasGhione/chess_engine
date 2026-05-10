# HydraY Chess Engine

HydraY is a C++23 chess engine with both a terminal interface and UCI support for
GUIs, bots, and automated testing. The engine is built around bitboards, magic
sliding-piece attacks, iterative deepening search, a cache-friendly
transposition table, and a handcrafted evaluator.

The project is mainly developed and tested on Linux/WSL, with a MinGW target for
Windows builds.

## Requirements

- `g++` with C++23 support
- GNU `make`
- OpenMP support (`-fopenmp`)
- Linux/WSL recommended for the full tooling suite
- Optional tools for analysis: `valgrind`, `clang-tidy`, `scan-build`,
  `include-what-you-use`, `cppclean`, `bear`, `perf`
- Optional for Windows cross-builds: `mingw-w64`
- Optional for evaluator tuning: Python 3, `chess-tuning-tools`, `cutechess-cli`,
  and an opening book under `tuning/books/`

## Quick Start

Build the engine:

```sh
make prod
```

Run the interactive terminal menu:

```sh
./chess
```

Run in UCI mode:

```sh
./chess uci
```

Example UCI session:

```txt
uci
isready
position startpos moves e2e4 e7e5
go depth 10
quit
```

The engine also auto-enters UCI mode when launched with piped stdin, which is
useful for GUIs and bot runners.

## Run Modes

```sh
./chess              # interactive menu
./chess uci          # UCI protocol mode
./chess -uci         # UCI alias
./chess --uci        # UCI alias
./chess -pvp         # human vs human
./chess -pvb w       # human vs engine, human plays White
./chess -pvb b       # human vs engine, human plays Black
./chess -bvb         # engine vs engine
```

In the terminal game mode, moves are entered with source and destination
squares, for example `e2 e4`. Promotions are entered with the promotion piece
when requested by the game flow.

## Build Targets

```sh
make                 # default build, produces ./chess
make chess           # explicit binary target
make prod            # optimized production build
make parallel_prod   # alias for prod
make prod_sequential # one-shot sequential production build
make debug           # debug build with symbols and profiling flags
make prod_windows    # cross-compile ./chess.exe with MinGW
make cls             # remove generated binaries, objects, and temporary files
make help            # show build-system help
```

Production builds use `-O3`, `-march=native`, `-mtune=native`, OpenMP, LTO,
loop unrolling, and section splitting. Object files are generated under
`output/`.

## Testing and Benchmarks

Build functional tests:

```sh
make test
./tests/test
```

Build performance tests:

```sh
make perf
./tests/perf
```

Run both functional and performance tests:

```sh
make all-tests
```

Run Valgrind leak checks:

```sh
make test-valgrind
```

Build and run the transposition-table huge-page benchmark:

```sh
make tt-huge-bench
./tests/tt_hugepage_bench --depth 10 --repeats 3
./tests/tt_hugepage_bench --depth 10 --repeats 3 --per-fen
```

Run the automated huge-page A/B benchmark:

```sh
./script/benchmark_tt_hugepages_ab.sh 10 3
./script/benchmark_tt_hugepages_ab.sh 10 3 output/bench_custom_run
```

If `perf` is available and permitted by the OS, the A/B script also records TLB
and CPU-counter metrics. On Linux, this may require:

```sh
sudo sysctl -w kernel.perf_event_paranoid=-1
```

## Huge Pages and Transposition Table

The transposition table stores 4 entries per 64-byte bucket and currently uses
1M buckets, for roughly 64 MiB of TT entries. On Linux, the engine can try to
allocate the table with explicit huge pages first, then transparent huge pages,
then normal heap allocation.

Control this with `CHESS_TT_HUGEPAGE`:

```sh
CHESS_TT_HUGEPAGE=auto ./chess      # default behavior
CHESS_TT_HUGEPAGE=on   ./chess      # force/try huge pages
CHESS_TT_HUGEPAGE=off  ./chess      # disable huge pages
```

Accepted enabled values are `on`, `1`, `true`, and `force`. Accepted disabled
values are `off`, `0`, and `false`.

## UCI Options

The engine exposes general UCI options:

```txt
PonderDebug          check, default false
SearchApiMutexGuard  check, default true
```

Examples:

```txt
setoption name PonderDebug value true
setoption name SearchApiMutexGuard value false
```

`SearchApiMutexGuard` can also be controlled at startup with:

```sh
CHESS_ENGINE_SEARCH_MUTEX_GUARD=false ./chess uci
```

Most evaluator and search-ordering constants from `engine/eval_constants.hpp`
are also exposed as UCI `spin` options. The displayed UCI names use CamelCase,
for example `PassedPawnBonus`, while tuning configs may also use the constant
names with underscores, for example `PASSED_PAWN_BONUS`.

## Evaluator Fine Tuning

HydraY can tune evaluator constants through self-play using
`chess-tuning-tools` and `cutechess-cli`. The tuning workflow lives in:

```txt
tuning/base_config.json     shared engine, depth, rounds, and book settings
tuning/groups/*.json        tracked parameter groups
tuning/tuning_config.json   generated/active experiment configuration
tuning/run_tune_local.sh    local launcher
tuning/chess_uci.sh         launches the engine in UCI mode
tuning/cutechess-cli        compatibility wrapper for local cutechess-cli
tuning/books/openings.pgn   opening suite used by cutechess
```

Install the external tools in a Python environment:

```sh
python3 -m venv tuning/.venv
source tuning/.venv/bin/activate
pip install chess-tuning-tools
```

Install `cutechess-cli` with your system package manager or from its upstream
build. Verify both tools are visible:

```sh
tune --help
cutechess-cli --version
```

Build the engine before starting a tuning run:

```sh
make prod
```

Run a tracked parameter group:

```sh
cd tuning
./run_tune_local.sh pawn_refine
./run_tune_local.sh threats_direct
./run_tune_local.sh king_attack_units
```

`run_tune_local.sh` tracks each dataset under `tuning/.tuning_state/` using the
group's `data_path`, `model_path`, and full config hash. A new dataset or a
changed config automatically gets `--no-resume --no-fast-resume`; an unchanged
dataset resumes, even if other groups were run in between.

### Tuning Config

Common settings live in `tuning/base_config.json`. Each tracked tuning group
lives under `tuning/groups/` and contains its own `parameter_ranges`,
`data_path`, and `model_path`. `run_tune_local.sh` merges the selected group
with the base config and writes the active `tuning_config.json`.

Available group files include:

```txt
pawn_refine.json
pawn_support_center.json
pawn_forks.json
material_values.json
threats_direct.json
threats_secondary.json
mobility_outposts.json
mobility_outpost_details.json
pinned_pieces.json
hanging_pieces.json
rook_activity.json
opening_fundamentals.json
phase_initiative.json
king_shelter_files.json
king_attack_units.json
king_safety_residual.json
king_activity_exposure.json
stalemate_draw.json
search_ordering.json
```

Keep each experiment focused: tune no more than about 8 related parameters at
once, and use a separate data/model pair for each group.

Example depth-limited experiment:

```json
{
  "parameter_ranges": {
    "LOW_MOBILITY_KNIGHT_PENALTY": "Integer(8, 12)",
    "PINNED_KNIGHT_PENALTY": "Integer(32, 38)"
  },
  "engine1_depth": 6,
  "engine2_depth": 6,
  "rounds": 1,
  "opening_file": "books/openings.pgn",
  "data_path": "mobility_data.npz",
  "model_path": "mobility_model.pkl"
}
```

For slower but more realistic testing, replace depth limits with time controls:

```json
"engine1_tc": "10+0.5",
"engine2_tc": "10+0.5"
```

Do not keep depth and time-control settings for the same engine at the same
time. For quick exploration use depth 6 or a short TC; for confirmation runs use
more rounds and a longer TC.

### Monitoring

Watch the tuner log:

```sh
tail -f log.txt
```

Watch games as PGN:

```sh
tail -f out.pgn
```

Check that processes are alive:

```sh
ps -ef | grep -E 'tune|cutechess|chess_uci|/chess' | grep -v grep
```

Generated tuning artifacts are intentionally ignored by git:

```txt
tuning/engines.json
tuning/log.txt
tuning/out.pgn
tuning/*.npz
tuning/*.pkl
tuning/plots/
```

### Reading Results

Important log lines:

```txt
Testing {...}          parameter point currently being tested
Got Elo: X +- Y        raw result of that tested point
Current optimum: {...} best point estimated by the Bayesian model
Estimated Elo: X +- Y  model estimate for the current optimum
```

Use `Current optimum`, not a single lucky `Got Elo`, when applying tuned values.
If the 90% confidence interval includes zero, the result is still uncertain and
should be confirmed with more games.

To apply tuned values, stop the run with `Ctrl+C`, copy the latest stable
`Current optimum` values into `engine/eval_constants.hpp`, rebuild with
`make prod`, then validate with a separate self-play or test run.

## How the Engine Works

The board is represented with compact square storage plus piece bitboards.
Sliding attacks use precomputed magic bitboard tables initialized at engine
startup. Zobrist hashing tracks positions for the transposition table and
repetition detection.

Search uses iterative deepening over alpha-beta/PVS. It includes quiescence
search, transposition-table probing/storing, killer/history/countermove/capture
history heuristics, move ordering, aspiration windows, null-move pruning,
reverse futility pruning, draw handling for stalemate/repetition/fifty-move
rules, and limited root parallelism.

The evaluator is handcrafted and split by piece/domain. It combines material,
piece-square tables, pawn structure, passed pawns, candidate passers, king
safety, king activity, rook and queen activity, mobility, outposts, threats,
hanging pieces, trapped pieces, coordination, bishop pair, castling, and game
phase blending. Constants live in:

```txt
engine/eval_constants.hpp
```

## Static Analysis

Run the full static-analysis suite:

```sh
make analyze
```

This can run cppcheck, clang-tidy, include-what-you-use, scan-build, GCC
analyzer, cppclean, and lizard depending on what is installed locally. Reports
are written to files such as:

```txt
analisi.log
scan-build-report/
complexity-report.csv
```

Run only complexity analysis:

```sh
make complexity
```

## Project Layout

```txt
board/        board representation, FEN, move execution, legality, bitboards
engine/       search, evaluation, engine runtime, constants
engine/eval/  evaluator modules by piece and feature
engine/search move generation, sorting, alpha-beta search
tt/           Zobrist hashing and transposition table
uci/          UCI protocol interface
driver/       terminal UI and game modes
tests/        functional, performance, and TT benchmark programs
script/       benchmark and analysis helper scripts
```

## License

See `LICENSE`.
