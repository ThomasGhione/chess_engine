#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "${script_dir}"
export PATH="${script_dir}:${PATH}"
export MPLCONFIGDIR="${MPLCONFIGDIR:-/tmp/hydray-matplotlib-cache}"

config="${1:-tuning_config.json}"
if [[ $# -gt 0 && ( "${config}" == *.json || -f "groups/${config}.json" || -f "groups/${config}" ) ]]; then shift; else config="tuning_config.json"; fi
if [[ "${config}" != */* && -f "groups/${config}.json" ]]; then config="groups/${config}.json"; fi
if [[ "${config}" != */* && -f "groups/${config}" ]]; then config="groups/${config}"; fi
if [[ "${config}" == groups/* ]]; then
  python3 -c 'import json,sys; b=json.load(open("base_config.json")); g=json.load(open(sys.argv[1])); b.update(g); json.dump(b, open("tuning_config.json","w"), indent=2); print("Using", sys.argv[1])' "${config}"
  config="tuning_config.json"
fi

data_path="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("data_path", "data.npz"))' "${config}")"
model_path="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("model_path", "model.pkl"))' "${config}")"

exec tune local -c "${config}" -d "${data_path}" --model-path "${model_path}" "$@"
