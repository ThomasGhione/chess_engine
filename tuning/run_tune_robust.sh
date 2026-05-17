#!/usr/bin/env bash
# Robust wrapper around run_tune_local.sh.
#
# chess-tuning-tools 0.9.5 aborts the WHOLE run if a single cutechess
# experiment yields no parseable games (parse_experiment_result -> np.diff
# on an empty 1-D array -> ValueError). `tune local` resumes from the saved
# *_model.pkl, so simply relaunching skips only that one bad experiment.
#
# Usage:  ./run_tune_robust.sh threats_direct
# Stop:   Ctrl-C (a clean Ctrl-C is treated as "stop", not a crash).
#
# Do NOT start this while another `tune local` for the same group is still
# running (they would fight over the model pickle).
set -uo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "${script_dir}"

max_restarts="${MAX_RESTARTS:-1000}"
backoff="${RESTART_BACKOFF:-10}"   # seconds between restarts
restarts=0

# A clean Ctrl-C should stop the loop, not trigger a restart.
trap 'echo "[robust] interrupted by user, stopping."; exit 130' INT TERM

while :; do
  echo "[robust] $(date '+%F %T') launching: ./run_tune_local.sh $*"
  set +e
  ./run_tune_local.sh "$@"
  rc=$?
  set -e 2>/dev/null || true

  if [[ ${rc} -eq 0 ]]; then
    echo "[robust] tune exited cleanly (rc=0). Done."
    exit 0
  fi
  if [[ ${rc} -eq 130 || ${rc} -eq 143 ]]; then
    echo "[robust] tune stopped by signal (rc=${rc}). Not restarting."
    exit "${rc}"
  fi

  restarts=$((restarts + 1))
  if (( restarts > max_restarts )); then
    echo "[robust] hit MAX_RESTARTS=${max_restarts}; giving up."
    exit "${rc}"
  fi
  echo "[robust] $(date '+%F %T') tune crashed (rc=${rc}); restart #${restarts} in ${backoff}s (resumes from *_model.pkl)..."
  sleep "${backoff}"
done
