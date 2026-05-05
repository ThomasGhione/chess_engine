#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "${script_dir}"
export PATH="${script_dir}:${PATH}"
export MPLCONFIGDIR="${MPLCONFIGDIR:-/tmp/hydray-matplotlib-cache}"

exec tune local -c tuning_config.json "$@"
