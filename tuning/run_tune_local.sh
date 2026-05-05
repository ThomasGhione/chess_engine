#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "${script_dir}"
export PATH="${script_dir}:${PATH}"
export MPLCONFIGDIR="${MPLCONFIGDIR:-/tmp/hydray-matplotlib-cache}"

data_path="$(python3 -c 'import json; print(json.load(open("tuning_config.json")).get("data_path", "data.npz"))')"
model_path="$(python3 -c 'import json; print(json.load(open("tuning_config.json")).get("model_path", "model.pkl"))')"

exec tune local -c tuning_config.json -d "${data_path}" --model-path "${model_path}" "$@"
