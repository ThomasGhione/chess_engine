#!/usr/bin/env bash
#
# run_sprt.sh — Sequential Probability Ratio Test for HydraY search/eval changes.
#
# Plays the current build (./chess) against a frozen baseline (./chess_baseline)
# under a time control, and lets cutechess-cli's native -sprt decide merge/no-merge:
# it stops automatically as soon as an H0/H1 bound is crossed.
#
# WHY TIME CONTROL (not fixed depth): a search/pruning change that cuts nodes shows
# no benefit at fixed depth — the win is reaching greater depth in the same time.
# Always SPRT search changes under tc=, never under fixed depth.
#
# ---------------------------------------------------------------------------
# WORKFLOW
#   1. Freeze a baseline BEFORE you start changing code:
#        make prod && cp ./chess ./tuning/chess_baseline
#      (or:  ./tuning/run_sprt.sh --snapshot   to freeze the current ./chess)
#   2. Make your change, then:  make prod
#   3. Run:  ./tuning/run_sprt.sh
#
# ---------------------------------------------------------------------------
# TUNABLE KNOBS (env vars; sensible defaults below)
#   TC          time control "moves/sec+inc" or "sec+inc"   (default 4+0.04)
#   ELO0 ELO1   SPRT hypotheses, ELO                          (default 0 and 5)
#   ALPHA BETA  SPRT error rates                              (default 0.05 0.05)
#   CONCURRENCY parallel games                                (default: nproc/2)
#   HASH        TT size MiB per engine                        (default 64)
#   THREADS     search threads per engine                     (default 1)
#   BOOK        opening book PGN                              (default books/openings.pgn)
#   MAXGAMES    hard cap on games (safety net)                (default 4000)
#
# Examples:
#   ./tuning/run_sprt.sh                          # default [0,5] gain test
#   ELO0=-3 ELO1=3 ./tuning/run_sprt.sh           # non-regression test (for cleanups)
#   TC=10+0.1 CONCURRENCY=4 ./tuning/run_sprt.sh
# ---------------------------------------------------------------------------

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
cd "${script_dir}"

new_bin="${repo_root}/chess"
base_bin="${script_dir}/chess_baseline"

# --- --snapshot: freeze the current ./chess as the baseline and exit ----------
if [[ "${1:-}" == "--snapshot" ]]; then
    if [[ ! -x "${new_bin}" ]]; then
        echo "error: ${new_bin} not found — run 'make prod' first." >&2
        exit 1
    fi
    cp -- "${new_bin}" "${base_bin}"
    echo "Baseline frozen: ${base_bin}"
    echo "  (from $(cd "${repo_root}" && git rev-parse --short HEAD 2>/dev/null || echo '?') / current ./chess)"
    exit 0
fi

# --- preflight ----------------------------------------------------------------
if [[ ! -x "${new_bin}" ]]; then
    echo "error: ${new_bin} not found — run 'make prod' first." >&2
    exit 1
fi
if [[ ! -x "${base_bin}" ]]; then
    echo "error: baseline ${base_bin} not found." >&2
    echo "  Freeze one first:  ./tuning/run_sprt.sh --snapshot" >&2
    echo "  (do this on the OLD code, before applying your change)." >&2
    exit 1
fi

# Resolve the real cutechess-cli, skipping the tuning/ forwarding wrapper.
real_cutechess=""
IFS=':' read -ra path_dirs <<< "${PATH}:/usr/local/bin:/usr/bin"
for d in "${path_dirs[@]}"; do
    c="${d}/cutechess-cli"
    if [[ -x "${c}" && "$(readlink -f -- "${c}")" != "$(readlink -f -- "${script_dir}/cutechess-cli")" ]]; then
        real_cutechess="${c}"; break
    fi
done
[[ -n "${real_cutechess}" ]] || { echo "error: real cutechess-cli not found in PATH." >&2; exit 127; }

# --- knobs --------------------------------------------------------------------
TC="${TC:-4+0.04}"
ELO0="${ELO0:-0}"
ELO1="${ELO1:-5}"
ALPHA="${ALPHA:-0.05}"
BETA="${BETA:-0.05}"
HASH="${HASH:-64}"
THREADS="${THREADS:-1}"
# The engine does NOT honour the UCI "Threads" option: it sizes its OpenMP YBWC
# pool from omp_get_max_threads(). Pin it via the env so a single-threaded SPRT is
# actually single-threaded (otherwise every engine grabs all cores and oversubscribes).
export OMP_NUM_THREADS="${THREADS}"
BOOK="${BOOK:-books/openings.pgn}"
MAXGAMES="${MAXGAMES:-4000}"
# Time-controlled games need 1 searcher per *physical* core (HT siblings sharing
# a core distort the clock), leaving one core for the OS/cutechess. Fall back to
# nproc/2 if lscpu is unavailable.
phys_cores="$(lscpu -p=Core,Socket 2>/dev/null | grep -v '^#' | sort -u | wc -l)"
[[ "${phys_cores}" =~ ^[0-9]+$ && "${phys_cores}" -ge 1 ]] || phys_cores="$(( $(nproc 2>/dev/null || echo 2) / 2 ))"
default_conc="$(( phys_cores - 1 ))"
[[ "${default_conc}" -ge 1 ]] || default_conc=1
CONCURRENCY="${CONCURRENCY:-${default_conc}}"

[[ -f "${BOOK}" ]] || { echo "error: opening book ${BOOK} not found." >&2; exit 1; }

stamp="$(date +%Y%m%d_%H%M%S)"
pgn_out="sprt_${stamp}.pgn"

echo "=============================================================="
echo " HydraY SPRT"
echo "   new      : ${new_bin}"
echo "   baseline : ${base_bin}"
echo "   TC=${TC}  H0=${ELO0} H1=${ELO1}  alpha=${ALPHA} beta=${BETA}"
echo "   threads=${THREADS} hash=${HASH}MiB concurrency=${CONCURRENCY}"
echo "   book=${BOOK}  pgn=${pgn_out}  cap=${MAXGAMES} games"
echo "=============================================================="
echo " H1 accepted (LLR>=upper)  -> the change is a GAIN: keep it."
echo " H0 accepted (LLR<=lower)  -> no measurable gain: discard it."
echo "=============================================================="

# -repeat: each opening played from both sides for fairness.
# adjudication speeds up decided games without biasing Elo.
exec "${real_cutechess}" \
    -engine name=new  cmd="${new_bin}"  arg=-uci \
    -engine name=base cmd="${base_bin}" arg=-uci \
    -each proto=uci tc="${TC}" \
        option.Threads="${THREADS}" option.Hash="${HASH}" \
    -openings file="${BOOK}" format=pgn order=random plies=16 \
    -repeat -games 2 -rounds "$(( MAXGAMES / 2 ))" \
    -sprt elo0="${ELO0}" elo1="${ELO1}" alpha="${ALPHA}" beta="${BETA}" \
    -resign movecount=3 score=500 \
    -draw movenumber=40 movecount=8 score=8 \
    -concurrency "${CONCURRENCY}" \
    -ratinginterval 10 \
    -pgnout "${pgn_out}"
