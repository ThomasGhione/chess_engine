#!/usr/bin/env bash
# Robust wrapper around run_tune_local.sh.
#
# Handles two failure modes:
#
# 1. CRASH: chess-tuning-tools 0.9.5 aborts the WHOLE run if a single
#    cutechess experiment yields no parseable games (parse_experiment_result
#    -> np.diff on an empty 1-D array -> ValueError). `tune local` resumes
#    from the saved *_model.pkl, so simply relaunching skips that bad
#    experiment.
#
# 2. FREEZE: cutechess-cli or a child engine process can deadlock silently
#    (pipe stall, UCI handshake hang, SMP lock). The process stays alive but
#    nothing is logged. A watchdog monitors log.txt; if it stops growing for
#    WATCHDOG_TIMEOUT seconds the entire process group is killed and the run
#    is restarted (resuming from *_model.pkl).
#    Set WATCHDOG_TIMEOUT=0 to disable the watchdog.
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
backoff="${RESTART_BACKOFF:-10}"        # seconds between restarts
watchdog_timeout="${WATCHDOG_TIMEOUT:-1800}"  # 30 min of log silence → freeze

# Resolve the log path from the group json (mirrors run_tune_local.sh logic).
_config="${1:-tuning_config.json}"
if [[ "${_config}" != */* && -f "groups/${_config}.json" ]]; then _config="groups/${_config}.json"; fi
log_file="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("log_path","log.txt"))' "${_config}" 2>/dev/null || echo "log.txt")"
restarts=0
watchdog_pid=""
child_pid=""
watchdog_killed=0  # set to 1 by watchdog so the main loop knows to restart

stop() {
  [[ -n "${watchdog_pid}" ]] && kill "${watchdog_pid}" 2>/dev/null || true
  echo "[robust] interrupted by user, stopping."
  exit 130
}
trap stop INT TERM

# Watchdog: polls log.txt size every 60 s; if unchanged for watchdog_timeout
# seconds, kills the child process group so the main loop can restart it.
start_watchdog() {
  local target_pid="$1"
  (
    local last_size=-1 stale=0 current_size
    while kill -0 "${target_pid}" 2>/dev/null; do
      sleep 60
      current_size="$(wc -c < "${log_file}" 2>/dev/null || echo 0)"
      if [[ "${current_size}" -eq "${last_size}" ]]; then
        stale=$(( stale + 60 ))
        if (( stale >= watchdog_timeout )); then
          echo "[robust-watchdog] $(date '+%F %T') log.txt silent for ${watchdog_timeout}s — killing frozen run (pid ${target_pid})"
          kill -- "-${target_pid}" 2>/dev/null || kill "${target_pid}" 2>/dev/null || true
          watchdog_killed=1
          exit 0
        fi
      else
        stale=0
        last_size="${current_size}"
      fi
    done
  ) &
  watchdog_pid=$!
}

while :; do
  echo "[robust] $(date '+%F %T') launching: ./run_tune_local.sh $*"
  set +e
  # Run in its own process group so the watchdog can kill the whole tree.
  setsid ./run_tune_local.sh "$@" &
  child_pid=$!
  if [[ "${watchdog_timeout}" -gt 0 ]]; then
    start_watchdog "${child_pid}"
  fi
  wait "${child_pid}"
  rc=$?
  [[ -n "${watchdog_pid}" ]] && { kill "${watchdog_pid}" 2>/dev/null || true; watchdog_pid=""; }
  set -e 2>/dev/null || true

  if [[ ${rc} -eq 0 ]]; then
    echo "[robust] tune exited cleanly (rc=0). Done."
    exit 0
  fi
  if [[ ${rc} -eq 130 || ${rc} -eq 143 ]] && [[ ${watchdog_killed} -eq 0 ]]; then
    echo "[robust] tune stopped by signal (rc=${rc}). Not restarting."
    exit "${rc}"
  fi
  watchdog_killed=0

  restarts=$((restarts + 1))
  if (( restarts > max_restarts )); then
    echo "[robust] hit MAX_RESTARTS=${max_restarts}; giving up."
    exit "${rc}"
  fi
  echo "[robust] $(date '+%F %T') tune crashed or froze (rc=${rc}); restart #${restarts} in ${backoff}s (resumes from *_model.pkl)..."
  sleep "${backoff}"
done
