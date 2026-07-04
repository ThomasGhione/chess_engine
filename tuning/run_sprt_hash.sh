#!/usr/bin/env bash
# Asymmetric self-play SPRT to isolate the effect of TT size (UCI Hash option).
#
# Both sides run the SAME ./chess binary; the only difference is option.Hash.
# This removes every code confounder: any Elo delta is purely the larger TT.
#
# The TT accumulates entries across all moves of a game (generation bumps, no
# clear), so a small TT only becomes a bottleneck at longer TC / deeper search.
# Hence the default TC here is much longer than the gain-SPRT default; at
# tc=4+0.04 a 64 MiB table barely saturates and the effect drowns in noise.
#
# Usage:
#   make prod && ./tuning/run_sprt_hash.sh
#   BIG=512 SMALL=32 TC=30+0.3 ./tuning/run_sprt_hash.sh
#
# Knobs (defaults):
#   BIG         hash MiB for the "big" side                    (default 256)
#   SMALL       hash MiB for the "small" side                  (default 64)
#   TC          time control                                   (default 60+0.6)
#   ELO0/ELO1   SPRT bounds (gain test on big-minus-small)     (default 0 / 5)
#   ALPHA/BETA  SPRT error rates                               (default 0.05)
#   THREADS     OMP threads per engine (keep 1 for clean A/B)  (default 1)
#   CONCURRENCY parallel games                                 (default cores-1)
#   MAXGAMES    hard cap                                        (default 4000)
#   BOOK        opening book                                   (default books/openings.pgn)
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bin="${script_dir}/../chess"

[[ -x "${bin}" ]] || { echo "error: ${bin} not found — run 'make prod' first." >&2; exit 1; }

fastchess_bin="$(command -v fastchess || true)"
[[ -n "${fastchess_bin}" ]] || { echo "error: fastchess not on PATH (~/.local/bin)." >&2; exit 127; }

BIG="${BIG:-256}"
SMALL="${SMALL:-64}"
TC="${TC:-60+0.6}"
ELO0="${ELO0:-0}"
ELO1="${ELO1:-5}"
ALPHA="${ALPHA:-0.05}"
BETA="${BETA:-0.05}"
THREADS="${THREADS:-1}"
export OMP_NUM_THREADS="${THREADS}"
BOOK="${BOOK:-${script_dir}/books/openings.pgn}"
MAXGAMES="${MAXGAMES:-4000}"

phys_cores="$(lscpu -p=Core,Socket 2>/dev/null | grep -v '^#' | sort -u | wc -l)"
[[ "${phys_cores}" =~ ^[0-9]+$ && "${phys_cores}" -ge 1 ]] || phys_cores="$(( $(nproc 2>/dev/null || echo 2) / 2 ))"
default_conc="$(( phys_cores - 1 ))"
[[ "${default_conc}" -ge 1 ]] || default_conc=1
CONCURRENCY="${CONCURRENCY:-${default_conc}}"

[[ -f "${BOOK}" ]] || { echo "error: opening book ${BOOK} not found." >&2; exit 1; }

stamp="$(date +%Y%m%d_%H%M%S)"
pgn_out="${script_dir}/sprt_hash_${stamp}.pgn"

echo "=============================================================="
echo " HydraY TT-size SPRT (self-play, asymmetric Hash)"
echo "   binary   : ${bin}"
echo "   big=${BIG}MiB  vs  small=${SMALL}MiB"
echo "   TC=${TC}  H0=${ELO0} H1=${ELO1}  alpha=${ALPHA} beta=${BETA}"
echo "   threads=${THREADS} concurrency=${CONCURRENCY}  cap=${MAXGAMES} games"
echo "   pgn=${pgn_out}"
echo " A positive result means the bigger TT is worth it at this TC."
echo "=============================================================="

exec "${fastchess_bin}" \
    -engine name=h_big   cmd="${bin}" args="-uci" proto=uci option.Hash="${BIG}" \
    -engine name=h_small cmd="${bin}" args="-uci" proto=uci option.Hash="${SMALL}" \
    -each tc="${TC}" \
    -openings file="${BOOK}" format=pgn order=random plies=16 \
    -repeat -games 2 -rounds "$(( MAXGAMES / 2 ))" \
    -sprt elo0="${ELO0}" elo1="${ELO1}" alpha="${ALPHA}" beta="${BETA}" model=normalized \
    -resign movecount=3 score=500 \
    -draw movenumber=40 movecount=8 score=8 \
    -concurrency "${CONCURRENCY}" \
    -ratinginterval 10 \
    -pgnout file="${pgn_out}"
