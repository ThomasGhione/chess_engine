#!/usr/bin/env bash
set -euo pipefail

DEPTH="${1:-8}"
REPEATS="${2:-2}"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="${3:-output/bench_tt_hugepages_${STAMP}}"

mkdir -p "${OUT_DIR}"

echo "[bench] building tt-huge-bench target..."
make tt-huge-bench >/dev/null

BENCH_CMD=(./tests/tt_hugepage_bench --depth "${DEPTH}" --repeats "${REPEATS}")

run_mode() {
  local mode="$1"
  local log_file="${OUT_DIR}/${mode}.log"
  local perf_file="${OUT_DIR}/${mode}.perf.csv"

  echo "[bench] mode=${mode} depth=${DEPTH} repeats=${REPEATS}"
  CHESS_TT_HUGEPAGE="${mode}" "${BENCH_CMD[@]}" | tee "${log_file}"

  if command -v perf >/dev/null 2>&1; then
    if CHESS_TT_HUGEPAGE="${mode}" perf stat -x, -e dTLB-loads,dTLB-load-misses,cycles,instructions "${BENCH_CMD[@]}" >/dev/null 2>"${perf_file}"; then
      echo "[bench] perf stat saved: ${perf_file}"
    else
      echo "[bench] perf stat unavailable for mode=${mode} (permission/support issue)."
      rm -f "${perf_file}"
    fi
  else
    echo "[bench] perf command not found."
  fi
}

extract_nps() {
  local log_file="$1"
  awk '{
    for (i = 1; i <= NF; ++i) {
      if ($i ~ /^nps=/) {
        split($i, a, "=");
        print a[2];
      }
    }
  }' "${log_file}" | tail -n 1
}

extract_perf_event() {
  local perf_file="$1"
  local event_name="$2"
  if [[ ! -f "${perf_file}" ]]; then
    return 1
  fi

  awk -F, -v ev="${event_name}" '
    $3 == ev {
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $1);
      print $1;
      exit;
    }
  ' "${perf_file}"
}

run_mode off
run_mode on

OFF_NPS="$(extract_nps "${OUT_DIR}/off.log")"
ON_NPS="$(extract_nps "${OUT_DIR}/on.log")"
NPS_DELTA="$(awk -v off="${OFF_NPS:-0}" -v on="${ON_NPS:-0}" 'BEGIN { if (off > 0) printf("%.4f", ((on - off) / off) * 100.0); else printf("nan"); }')"

OFF_DTLB_MISSES="$(extract_perf_event "${OUT_DIR}/off.perf.csv" "dTLB-load-misses" || true)"
ON_DTLB_MISSES="$(extract_perf_event "${OUT_DIR}/on.perf.csv" "dTLB-load-misses" || true)"
DTLB_DELTA="n/a"
if [[ -n "${OFF_DTLB_MISSES}" && -n "${ON_DTLB_MISSES}" ]]; then
  DTLB_DELTA="$(awk -v off="${OFF_DTLB_MISSES}" -v on="${ON_DTLB_MISSES}" 'BEGIN { if (off > 0) printf("%.4f", ((on - off) / off) * 100.0); else printf("nan"); }')"
fi

echo
echo "=== TT HugePage A/B Summary ==="
echo "output_dir=${OUT_DIR}"
echo "off_nps=${OFF_NPS}"
echo "on_nps=${ON_NPS}"
echo "nps_delta_percent=${NPS_DELTA}"
if [[ -n "${OFF_DTLB_MISSES}" || -n "${ON_DTLB_MISSES}" ]]; then
  echo "off_dTLB_load_misses=${OFF_DTLB_MISSES:-n/a}"
  echo "on_dTLB_load_misses=${ON_DTLB_MISSES:-n/a}"
  echo "dTLB_load_misses_delta_percent=${DTLB_DELTA}"
else
  echo "dTLB_load_misses=unavailable"
fi
